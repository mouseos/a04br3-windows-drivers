# MT8167 PowerVR power sequencing foundation

## 実機試験の状況（2026-11-04）

WinPEで試験版KMDF providerの読込み、GPU firmwareの起動フラグ、および
KCCB health要求後の実行カウンターの進行を確認しました。実ハードは
22.40.54.30、試験に使用したfirmwareは22.67.54.30です。互換性は
確立しておらず、不一致の許可は試験ビルドだけに限定します。

`141466c` の3コアWinPE一括試験では表示miniportのPnP開始と、GE8300を
経由した16 KiB表示コピー2回の完了（失敗0回）を確認しました。Windowsの
画面表示もフレームバッファで確認しました。転送はCPU側のステージングを
含み、直接フレームバッファへGPUが書く方式ではありません。3D、速度改善、
長期安定性、スリープ復帰、および搭載Windowsへの統合は未確認です。

`render_stream.c`はLinux PowerVRのgeometry/fragmentストリーム形式を
読み取る基礎部品です。GPUアドレスの安全性、ジョブ投入、3D描画、Direct3D
ドライバを実装するものではなく、実機で検証されていません。対応する拡張は
限定され、BRN47217など未対応quirkを要求する構成では使用できません。
`render_freelist.c`はレンダー用PMページの固定長リストを構成する部品で、
ページの所有・同期、HWRT、描画ジョブの発行は呼出し側が実装する必要があり、
実機では未検証です。
`render_hwrt.c`はGE8300のSIPF1・16×16タイル・単一サンプル・単一レイヤー
に限定したHWRT設定の試作です。GPUメモリの領域・寿命・同期、ジョブ発行、
実機動作は別途必要で、この部品自体は3D描画を行いません。
`render_context.c`はgeometryとfragmentのファームウェア用コンテキストと
空キューを準備します。ジョブの依存関係、送信、完了、エラー復旧、OSのGPU
メモリ管理は未接続で、実機描画の確認にはなりません。

30d0088のWinPE実機試験では、idle・power-off応答、未処理転送の確認、
ファームウェアプロセッサのreset読戻しを通り、DMA割当を解放しました。
独立したバス測定や長期安定性は未確認です。失敗時の共有メモリは保持して
リセットします。GPUによる固定画像のクリアは下記のWinPE試験で成功しました。
完全なWDDM/3D、PnP電源遷移、スリープ復帰は未完成です。共通DMAバッファのキャッシュ整合性、特に異なる
所有者が同じキャッシュラインを共有する場合の安全性も未検証です。

`gpu_mmu.c` はGE8300の３段ページテーブルへ4 KiB単位の連続または散在する
DMAページの設定・解除を行う部品です。範囲・重複を変更前に検査します。WinPE試験には
DMA領域の確保とテーブルのキャッシュ同期があります。汎用のジョブ管理とWDDM連携は未実装です。
firmwareへのMMUキャッシュ無効化要求は実行カウンターと専用同期値の両方で
完了を判定し、a0416c9のWinPE実機試験で成功しました。aea6083の固定画像クリアで
GPUによるページ変換と書込みを確認しました。ページの実行中解除は未検証です。
`a04_gpu_mmu_replace_pages_4k` は既存のleaf範囲と新DMAページを全て検査してから
置換します。ジョブ終了、旧DMAの保持、テーブル同期とMMU無効化完了は呼出し側の
責任です。184c8d6のWinPE試験で保持済みDMAページ間の置換を５回検証しました。
OSのscatter/gather DMA mappingとWDDM DMA連携は未実施です。
`dma_pages.c` はOSから取得済みのscatter/gather DMA範囲を4 KiBページ列に
検証・変換する部品です。OSのDMA取得・解放、ページ固定、キャッシュ同期、
GPUジョブ寿命の管理は行わず、WDDMにはまだ接続していません。

12ccaeeのWinPE実機試験では、BRN65273向け２領域を含むGPUページテーブルと
firmware VMを準備し、compute NULL/update queueのフェンス応答、power-offと
DMA割当解放まで確認しました。これはfirmwareのキュー処理であり、シェーダの
実行、GPU仮想アドレスの実変換、描画、WDDMの動作を示しません。

`rogue_power.h` is derived from the BSD-2-Clause-Patent
[UniXNU A04BR3 implementation](https://github.com/mouseos/UniXNU-a04br3/blob/main/package/A04BR3RoguePower.h),
which in turn identifies the board Linux clock and power sources. Its callback
interface is a starting point for a Windows port, not a WDDM driver. It is not
connected to PnP, MMIO ownership, clocks, firmware, GPU memory, interrupts or
Direct3D, and must not be applied to a running device on its own.

Power and clock registers are shared with other devices, including touch.
A Windows resource and synchronization design is required before any access.
The optional MMPLL calculation uses 64-bit division; kernel linking on ARM32
has not been established. Tests use mocks only; GPU acceleration is not yet
implemented or verified on Windows RT.

## Rogue MIPS firmware image loading foundation

`firmware.c` validates the open firmware info, layout and 32-bit little-endian
MIPS ELF program headers, then loads the code and data segments into a caller's
16 MiB firmware heap. It follows the upstream PowerVR firmware layout from
[Linux pvr_fw_info.h](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/gpu/drm/imagination/pvr_fw_info.h)
and the board-specific [UniXNU boot implementation](https://github.com/mouseos/UniXNU-a04br3/blob/main/package/powervr/NativePowerVR.cpp).
The firmware is a separate input and is not stored in this repository. The
board implementation uses Imagination's `rogue_22.67.54.30_v1.fw` (SHA-256
`cfe6f58db2ad604b1729349629510c5a3434b266c99ea886d2959887bff4fed8`):
https://gitlab.freedesktop.org/imagination/linux-firmware/-/raw/powervr/powervr/rogue_22.67.54.30_v1.fw

The actual board core is 22.40.54.30. A mismatch is rejected by default and
can only be allowed explicitly; successful image loading does not establish
that the firmware is compatible. This portable code does not allocate DMA
memory, start the firmware, register a Windows device or implement WDDM.
It can also prepare the MIPS boot configuration and the four-page GPU page
table from separately allocated, page-aligned physical heap and table memory
below 4 GiB. The Windows caller must provide and synchronize those allocations;
the prepared data is used by the opt-in firmware boot test described below.
The optional KMDF provider `mt8167-ge8300.inf` binds the ACPI resource and
offers a status query. Its normal build does not start firmware. The explicitly
enabled `A04_GE8300_BOOT_TEST=1` WinPE build attempts one firmware boot,
health check and shutdown; `A04_GE8300_ALLOW_MISMATCH=1` separately permits
the known firmware/core mismatch. It is not WDDM and is not intended for the
installed Windows system. Firmware shutdown reached the successful path once;
surprise removal, sleep and the long-term safety of DMA handling remain unverified.
`tools/ci/build-ge8300-provider.ps1` fetches the pinned firmware or accepts
`A04_GE8300_FIRMWARE`; in both cases it verifies the SHA-256 before packaging.
`firmware_runtime.c` also constructs the firmware queues and shared system and
OS initialization data. Its ABI structures come from the unchanged upstream
MIT-licensed FWIF headers listed in `fwif/SOURCE`; the Linux compatibility shim
is derived from UniXNU and adjusts `GENMASK` for 32-bit `unsigned long`.
`firmware_start.c` expresses the board's MIPS reset and remap sequence through
MMIO callbacks; returning success only means the sequence was issued. The
experimental Windows adapter uses split MMIO and barriers, and the opt-in
test has observed the firmware-started flag. Cache synchronization, power
transitions and long-term resource ownership still require verification.
An additional, default-off WinPE-only build option `A04_GE8300_CLEAR_TEST=1`
requires both boot and mismatch opt-ins. It uses a fixed 64x64 RGBA8 transfer
generated by Mesa PowerVR at ef11349e8e, with its PDS event and USC EOT
programs. The provider maps only their pages and a 16 KiB destination in a
private GPU address space, kicks a TQ3D queue, waits for its firmware fence,
and checks every output byte. A timeout or mismatch retains DMA allocations;
this diagnostic is not a general command interface or WDDM. The device's
released Linux BVNC table identifies four ISP IPP pipes. The firmware and
fixture use 22.67.54.30 while the core is 22.40.54.30: hardware compatibility,
DMA containment, cache behaviour and long-term operation remain to be verified.
On TAB-A04-BR3 WinPE with aea6083, one opt-in test completed with QUEUE=0,
SHUTDOWN=0 and STATUS_EXIT=0. In this build QUEUE=0 requires a completed
fence and an exact comparison of all 16,384 image bytes. This establishes
one fixed GPU memory write and readback; it does not establish copy, display,
WDDM, production DMA isolation or long-term stability.
`A04_GE8300_COPY_TEST=1` selects a separate opt-in image-to-buffer transfer
probe; it cannot be combined with the clear probe. A fixed Mesa-generated
stream and its sparse GPU address-space snapshot are retained in
`copy_fixture.c` (Mesa PowerVR ef11349e8e, MIT). A varying eight-byte value
resembling a host pointer was removed; GPU use of that location is unverified.
The two image regions are
initialized by the test itself. It maps private buffers, submits one transfer,
and checks every destination byte. On TAB-A04-BR3, c8559a3 completed one
WinPE test with QUEUE=0 and SHUTDOWN=0, including comparison of all 16,384
output bytes after the GPU fence. The source was a uniform color, so pixel
ordering, orientation, general copy dimensions, display integration and
long-term safety are not established by this result.
`A04_GE8300_LINEAR_TEST=1` selects a further fixed, nonuniform 16 KiB linear
buffer-to-buffer copy, generated by the same Mesa version. It compares every
source and destination byte, including untouched 4 KiB prefix and suffix
regions. It is mutually exclusive with the other diagnostics and has not yet
been connected to WDDM. On TAB-A04-BR3, 86b3031 completed one WinPE test
with QUEUE=0 and SHUTDOWN=0: all copied bytes matched and both the source and
destination guard regions remained unchanged. This is one fixed transfer,
not a general WDDM submission interface or a long-term stability result.
The current linear test additionally reuses the same queue for 145 further
copies with varying payloads and fence values, crossing the ring boundary.
Each result and guard is checked. One WinPE run of 150e569 passed all 146
transfers and clean shutdown. Concurrent jobs, cancellation, display output
and long-term operation remain unverified.
The current test also switches both buffers between two sets of pages in a
retained DMA allocation five times, synchronizes the leaf table and waits for
firmware MMU invalidation at each switch. One WinPE run of 184c8d6 passed
this sequence. Real OS scatter/gather mappings and concurrent use remain untested.
The internal transfer queue can also append another command after the caller
has synchronized and observed both the previous fence and an empty queue.
It handles ring padding at wrap and increments the fence; the caller must
validate GPU addresses, serialize work and synchronize the ring before the
kick. Reuse passed the fixed WinPE sequence above; it is not connected to
Windows desktop presentation or a full graphics driver.
`windows_hw.c` provides an experimental Windows ARM32 adapter: common DMA
buffers use the device adapter's logical addresses, and split 32-bit MMIO
writes follow the low-then-high order in this device's released Linux source.
Common-buffer cache policy is selected by Windows, so the firmware boot and
queue paths explicitly synchronize device reads and writes with MDLs and
`KeFlushIoBuffers`; that behavior still needs verification on this hardware.
The experimental KMDF provider acquires the device resources and DMA adapter;
WDDM ownership, GPU quiescence before free and production power management
are not implemented or verified.
`windows_firmware.c` composes these steps and waits for the firmware-started
flag. The opt-in KMDF WinPE test invokes it on the device as described above.
After issuing hardware writes it retains DMA memory even on timeout: a caller
must first establish that GPU DMA is quiescent before releasing that memory.
The shutdown helper sends firmware idle and power-off requests, then checks
GPU idle and outstanding transactions before reset. It frees allocations only
after a successful reset readback. A timed-out command poisons the queue and
retains DMA memory. The Windows PnP power lifecycle and all hardware behavior
still need implementation and device testing.
For a host check with that exact firmware:

```
cc -std=c11 -Wall -Wextra -Werror -I drivers/gpu/mt8167 -I drivers/gpu/mt8167/compat -I drivers/gpu/mt8167/fwif tests/gpu_firmware_image_test.c drivers/gpu/mt8167/firmware.c drivers/gpu/mt8167/firmware_runtime.c drivers/gpu/mt8167/firmware_start.c -o /tmp/a04-gpu-fw-test
/tmp/a04-gpu-fw-test /path/to/rogue_22.67.54.30_v1.fw
cc -std=c11 -Wall -Wextra -Werror -I tests/gpu_windows_stubs -I drivers/gpu/mt8167 -I drivers/gpu/mt8167/compat -I drivers/gpu/mt8167/fwif tests/gpu_windows_firmware_test.c drivers/gpu/mt8167/firmware.c drivers/gpu/mt8167/firmware_runtime.c drivers/gpu/mt8167/firmware_start.c drivers/gpu/mt8167/windows_hw.c drivers/gpu/mt8167/windows_firmware.c drivers/gpu/mt8167/gpu_mmu.c drivers/gpu/mt8167/firmware_objects.c drivers/gpu/mt8167/firmware_queue.c drivers/gpu/mt8167/windows_queue_test.c -o /tmp/a04-gpu-win-test
/tmp/a04-gpu-win-test /path/to/rogue_22.67.54.30_v1.fw
```
# DMA page replacement for fixed linear transfers

`transfer_mapping` and `windows_transfer_mapping` encapsulate the tested
six-page source and destination VA replacement sequence. The Windows adapter
waits for both the fence and the queue read offset, cleans the leaf table and
waits for firmware MMU invalidation. The caller must serialize access, supply
OS-owned DMA mappings and exclude every GPU-owned DMA range. On an ambiguous
failure the mapping is poisoned and old and new memory must remain allocated
until GPU DMA has stopped. External mappings can be released only after a
successful scratch restore and invalidate acknowledgement. The helpers do not
acquire Windows scatter/gather mappings, synchronize external buffers, submit
jobs, implement power management or connect the WDDM presentation path.

The existing linear diagnostic now exercises these helpers in the simulated
test; this integration has not yet been verified on the device.

`windows_linear_context` packages the isolated page tables, fixed command,
firmware objects, first queue submission and initial fence into a reusable
context. The host simulator exercises initialization, a later transfer and
the retained-allocation path on timeout. The command remains a diagnostic
fixed-size fixture with the BVNC mismatch described below. The optional WDDM
display-only client was exercised in WinPE as described above. Do not ship it as a
graphics driver; firmware compatibility, PnP and power management remain open.
The opt-in linear WinPE diagnostic now initializes through this context and
retains its DMA allocation until the existing firmware shutdown sequence.
Its fixed 16 KiB staging-copy API copies through private DMA common buffers,
checks protected ranges, and does not expose the caller's buffer to the GPU.
The output is left untouched on error. This CPU-staged path is an integration
building block, not evidence of faster display or direct framebuffer DMA.

An opt-in `A04_GE8300_COPY_SERVICE` path provides one synchronous buffered
16 KiB copy request to kernel callers through the existing GE8300 provider.
It explicitly requires the test firmware's BVNC-mismatch opt-in and is off
by default. The sequential power-managed WDF queue drains requests before D0
exit; the provider then requests GPU shutdown and only releases its DMA on a
successful stop. An unsuccessful shutdown retains the allocations. After D0
exit it stays unavailable on resume until a fresh PnP device instance starts;
SoC power restoration is not implemented. The optional display miniport client
is tested only in WinPE and must not be treated as a supported driver. The
initial WinPE test path remains separate. See Microsoft's [power-managed
queues](https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/using-power-managed-i-o-queues).

## MT8167 対応 m1.11 ファームウェアの準備

実コア 22.40.54.30 向け vendor m1.11 ファームウェアの版1レイアウトを検証し、WindowsのDMA共通バッファにコード・データ・MIPSページ表とm1.11用の初期FWIFキューを準備する経路を追加した。`A04_GE8300_VENDOR_STAGE=1` は既定オフで、既存の版3ファームウェア起動試験と同時には使えない。ビルド時は `A04_GE8300_FIRMWARE` に正規に用意した版1ファームウェアを指定し、期待するSHA-256を検査する。実機側の入力名は `System32\drivers\ge8300-mips.fw`。専用IOCTLの `stage_status` はCPU準備と解放の結果であり、従来の `boot_status` と `health_status` は `STATUS_NOT_SUPPORTED` のまま。GPUファームウェア起動、FWIFとの実行時通信、描画、DMA停止は実機確認していない。

XNUのm1.11起動手順を使うWindows向けトランスポートを追加したが、現在のPnPドライバからは呼び出さない。電源・クロックと割り込みマスクの所有、DMAエネーブラの寿命、停止時のDMA排出を統合してから実機で使用する必要がある。起動試行後の共通バッファは成功・失敗を問わず保持し、未実装の停止経路で解放しない。

実装は [mouseos/xnu_kernel_benesse_a04br3](https://github.com/mouseos/xnu_kernel_benesse_a04br3) の `iokit/Kernel/arm/rogue/A04BR3RogueFirmware.h`、`A04BR3RogueMipsMMU.h`、`A04BR3RogueMemory.h`、`A04BR3RogueRuntimeLayout.h`、MITライセンスの `A04BR3RogueDDKABI.h` を参照し、BSD-2-Clause-Patentのヘッダを保持している。`make test-gpu-vendor-m1-11` は合成画像で正常系と破損・DMA同期失敗を試す。該当vendor firmwareを別途入力したホスト検証でも独立した版1展開結果と一致したが、配布権と実機動作は別途未確認。

m1.11向けの固定SYNCコマンド経路を追加した。GPU側のキュー読取りと同期値の両方を確認し、タイムアウトや破損後はキューを再利用しない。現時点ではPnPから呼び出さず、実機でのファームウェア通信を意味しない。コマンド制御領域はCPUとGPUが同じキャッシュ行へ同時に書くため、キャッシュ無効またはハードウェア整合性の証拠が必要である。Microsoftの[AllocateCommonBuffer仕様](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pallocate_common_buffer)では `CacheEnabled` 引数は無視され、ARMではACPIの `_CCA` などに基づきOSが決定する。`FALSE` を渡しただけでは証明にならない。対象環境で属性とDMA整合性を確認するまでは、このコマンド経路を実機で有効にしない。

端末固有UEFI `46d18c37` は、GPU電源状態とクロック選択を読み取るACPIメソッド `GPST` を提供する。Windows側は応答の形式・長さと電源状態を確認してから、割り当て済みMFGリソースのクロック状態を読む。これは読み取り時点のスナップショットで、電源状態の維持や共有レジスタの所有を保証しない。実際のPnP経路には未接続であり、メソッドのWindows実行は未確認。

`A04_GE8300_VENDOR_STAGE=1` の診断構成は、PnPから `GPST` 読取りとMFG状態の取得を試し、専用IOCTLとUARTへ結果を出す。読み取りに失敗してもCPU側のファームウェア準備結果とは別に記録する。未対応UEFIではメソッド取得失敗を報告し、取得成功も電源状態の継続やGPU動作を保証しない。

m1.11の停止準備として、Linux DDKの `RGXPrePowerState` に従い固定の電源オフ要求をキューへ送り、消費位置、power sync、FWのOFF状態を確認する処理を追加した。タイムアウト時はキューを使用不能にする。FW応答は割り込み排出やバスのDMA停止を証明しないため、成功後もDMAメモリを保持する。この経路はPnPに未接続で、実機検証はしていない。

m1.11のRGXStopに対応するレジスタ停止処理も追加した。FWのOFF応答とIRQ排出を前提に、Sidekick・SLCのidle、BIF/MMUと外部読取りの状態を確認し、MTSの関連付けを解除する。レジスタ条件の成功はSoC外側までのDMA排出を保証しないため、DMA領域を解放しない。割り込みカウンタ排出とPnPからの呼出しは未実装で、実機検証はしていない。

## m1.11 WinPE実機診断（2026-11-05）

上記の未接続・未検証という記述は各追加時点の記録。端末専用UEFI `3a310ccc` と4 CPUのWinPEで、ドライバ `2d144a9` の診断経路はDMA v3アダプターの取得、非キャッシュ領域への準備、GE8300ファームウェア起動、GP SYNCの完了をUARTで確認した。`DEVICE_DESCRIPTION_VERSION3` では `Dma32BitAddresses` は無視され、`DmaAddressWidth` に非ゼロを指定する必要がある。詳細は [Microsoftの構造体仕様](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_device_description)。

続く `0f8cd0e` の実機診断では電源オフ要求後の状態は同期値1、電源状態IDLE、KCCB読取り・書込み位置とも2だった。Linux m1.11 DDKは応答後にOFFでなければ電源移行拒否として扱うため、`bb71457` はキュー消費・応答を確認した上でOFF以外ならビジーを返し、オンライン状態とDMA所有を維持する。停止後の安全なDMA解放、共有電源の所有、GPU描画および通常Windowsへの導入は未確認。WinPE専用の診断フラグは既定OFFのままであり、スナップショットは電源所有やDMA停止の証明ではない。

`vendor_m1_11/clear_tile.c` は既存 32×32 zero-clear に対して、所有済み
64 KiB 内の32画素境界タイル位置と32画素倍数pitchを指定するCPU上の生成部品。
XNU `A04BR3RogueClearTile.h` と REA `ev_a8b70e873cd2536b115a348ff0bc99a5802b311ce09299c99f127d837cf5ef13`
の native format 0x22・線形PBEアドレスとpitchに基づく。32×32外の矩形、
任意色、pitchの端数、64 KiBを超える全画面、資源管理、GPU送信は対象外。
成功は画素出力を意味せず、現行の固定診断に接続しない。Windows実機の
可視性、転送完了、所有・停止・画面出力は未検証。

`A04_GE8300_VENDOR_TILE_DIAGNOSTIC=1` は既定OFF。既存のvendor stage、
uncached stage、IRQ、start、render cold、clear診断の全指定を必要とし、
カラー診断とは併用しない。64 KiB専用領域を0xa5で初期化し、pitch 128画素、
位置(64,64)の32×32だけをzero-clearする。対象1024画素と残り15360画素の
すべてをSLC flush・fence成功後に照合する。タイムアウト時の読取りは暫定値で、
DMA停止や領域解放の証明ではない。対象実機・Windows本体への適用は未検証。

### m1.11 context cleanup and power-off

In the 98dea76 run at UART offset 16720324, all four context commands and
the bounded 32x32 transfer reported success, but the power-off request returned
`80000011`; the snapshot showed power sync 1, power state 1, KCCB read=write=9
and FWCCB read=write=0. A later color-clear run showed the same result. These
are observations, not proof that live contexts caused the rejection.

The experimental m1.11 path now drains each known client CCB and sends the
vendor DDK common-context CLEANUP command on its data master. RUN without BUSY
and consumption of the KCCB entry are required for each context before the
power-off request. BUSY leaves memory owned; timeout or invalid queue state
poisons the context and stops further submissions. Partial cleanup forbids new
work. Firmware cleanup and power acknowledgement do not prove external fabric
quiescence; all DMA stays pinned and D0 exit remains guarded. This change is
host-tested only; actual m1.11 cleanup and power-off are not yet verified.

`A04_GE8300_VENDOR_MULTI_TILE_DIAGNOSTIC=1` は既定OFFで、既存tile診断を
前提とする。同一起動中、専用64 KiBを毎回guardで再初期化し、(64,64) zero、
(32,32) zero、(96,96) 0x78563412の順に処理する。各回の完了・SLC flush・
fenceと全画素照合が成功した場合のみ次へ進み、失敗後は再使用しない。
最初のzeroはcc1be36で実測済みだが、再初期化・PDS更新・連続実行・色付きの
新経路は未検証。PDSやデバイス書込みの可視性・実GPUの色変換はhost模擬では
保証できない。FW cleanup成功はDMA停止を証明しない。表示用共有フレーム
バッファのpin、GPU/scanout ownershipとfence、電源・PnP寿命は未実装であり、
この診断の64 KiBを表示と共有しない。

### m1.11 forced-idle電源停止診断

`cc1be36` ではユーザー提供の実機結果として、オフセット付きclearのMATCH=400、GUARD=0、PHASE=4、４コンテキストのCLEANUP STATUS=0・MASK=Fだった。その後OFF=80000011、SYNC=1、POW=1、KRD=KWR=0Dであり、cleanup不足単独では拒否を説明できない。FWのstate 1はONである。

参考のMediaTek Linux DDK m1.8 `services/server/common/power.c` のシステムOFF経路は、forced-idleを要求してから強制電源OFFを要求する。同DDK `rgxpower.c` は同期応答後にFORCED_IDLEを検査する。m1.11の固定FWIF宣言には別の `RGXFWIF_POWER_FORCE_IDLE=1` とforced OFFのフィールドがあり、m1.8のboolean値をそのまま流用しない。本診断はcleanup成功後にm1.11のforced-idleを送信し、同期値・KCCB消費・FW状態FORCED_IDLEをすべて確認できた場合だけforced OFFを送る。変更は既定OFFのvendor start診断のみに接続する。idle拒否、timeout、FW要求保留、予期しない状態ではOFFへ進まず、再投入を封じてDMAを保持する。

ホストmockでidle拒否、forced OFF拒否、両段階成功、idle timeoutを検証した。現時点でWindows実機のforced-idle/OFFは未検証であり、この順序が拒否の原因を解消するかは不明。FWのOFF応答とstop registerの確認も外部fabricを含むDMA静止や領域解放を証明しない。

`a04_gpu_mmu_map_pages_multi_4k` はOSから既に取得・保持されたDMAアドレス列を、
最大16個の4 KiB leaf（32 MiB、単一1 GiB directory内）に展開する準備部品。
全leafのDMA・CPU領域と全エントリを先に検査し、途中の異常で部分更新しない。
`a04_gpu_dma_pages` は画面規模を扱うため最大8192ページを受け付ける。どちらも
CPU上の構築のみで、OSのDMA取得、POST framebufferのpin、キャッシュ同期、GPU MMU
invalidate、描画fence、scanout、取消し、停止・解放を行わず、既定経路には未接続。
MicrosoftのGetScatterGatherListは対応するPutScatterGatherListとDMA方向・同期を
要求し、POST物理アドレスをGPU DMAアドレスとして直接扱う根拠にはならない。

`vendor_m1_11/windows_surface.c` は OS の version-3 DMA adapter による
common buffer と複数leaf GPU page tableを、GPU未起動状態で準備する未接続の
部品。832×1280・4byteの領域を含む最大8 MiB、最大4 leafに制限する。DMA
logical addressとCPUアドレスは別であり、adapterは最後まで呼出し側が保持する。
ハードへ公開する前に `mark_exposed` を呼ぶ契約で、その後は現在解放を拒否する。
この部品はWDDMや表示コントローラへ公開せず、任意のboolで停止・fence完了を
主張しない。OSのcommon bufferはGPU adapterに対する権利だけで、scanoutやPOST
framebufferへの共有は証明しない。ARMのcache属性、両DMA主体のmappingと所有、
GPU fence/MMU invalidate、表示走査の完了、PnP停止後の解放は未実装・未検証。

`A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC=1` は既定OFFで、vendor start・cold・
zero tile診断を要し、既存multi-tileとカラー診断には併用しない。GPU未起動時に
専用832×1280 pitchの4,259,840-byte common bufferを別GPU VAにマップし、既存
transfer contextの未使用catalog slotへ接続する。800×1280領域の右下32×32を
zero-clearし、fenceとSLC flush後に対象1024画素とそれ以外の全画素を比較する。
画面走査やWDDM framebufferは使わない。GPU起動前にsurfaceを公開済みに固定し、
起動失敗やforced-idle／FW cleanup成功後もDMAとadapterを保持し、D0 exit・releaseを
拒否する。これらの拒否はPnP削除時のリソース寿命や物理除去を保証しない。
高pitch、複数leaf、キャッシュ属性、デバイス停止は実機未検証。実行結果を
画面描画・共有・DMA排出の証拠としない。

WDDMのCommitVidPnはVidPnコールバックを取得・検証後、Presentと同じmutexで
framebufferのunmap、mapとmode更新を直列化する。mutex保有中にVidPnの取得・
解放コールバックを呼ばない。visibilityとactive pathの変更も同じmutexを使い、
停止後は拒否する。[CommitVidPn](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_commitvidpn)、
[PresentDisplayOnly](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_presentdisplayonly)
はPASSIVE_LEVELで、後者はzero-level同期。これはGPU DMAやscanoutの停止を保証しない。
bugcheck用SystemDisplayの高IRQL経路にmutexを導入しておらず、その寿命は別途検証が必要。

既定OFFの旧ABI WDDM copy実験では、同期IOCTLの出力を専用nonpaged stagingに
受け、成功かつ所定長の完了後のみPOST mappingへ32-bit register accessで転記する。
部分応答・エラーはその矩形全体のCPU経路に戻す。framebufferをIOCTL出力先やGPU
DMA先として渡さない。これは旧ABIの転送経路であり、m1.11の一般描画やscanout
共有、GPU停止時のDMA静止・解放を証明しない。実機での可視性は未検証。
[WRITE_REGISTER_BUFFER_ULONG](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-write_register_buffer_ulong)
は常駐バッファからmapped device memoryへULONG列を書き、memory barrierを挿入する。
同期Presentでmutexを保持している間に転記を終え、モード変更・停止と直列化する。
