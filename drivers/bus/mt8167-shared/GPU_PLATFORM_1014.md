# GPU platform owner 1014

基点はecaa7b70248230dee2977e1d9057260f400287be（parent1008）。
AFE BUS_INTERFACE_STANDARD転送、EnginePCM/USB/WiFiの実flagを保持。
Raman正本は編集せずwork/ge8300-platform-owner-1014で作業する。

`A04_SHARED_GPU_PLATFORM`は既定0。意図した1014 profileだけ1。
`prototype/gpu-parent1014-profile.json`は実1008 parent-manifestのflagを
そのまま保持しGPU flagだけ追加。INF DriverVerは2026.11.7.1014。
ビルドは`tools/ci/build-gpu-parent1014.ps1 -EnableCandidate
-ExpectedSourceCommit <fixed> -OutputDirectory <checkout内の新規dir>`。
署名・実機deploy・GPU停止成功を意味しない。

## 現在の正規割当で可能な操作

親のraw/translated割当はTOP0x10000000、INFRA0x10001000、GPIO0x10005000、
PWRAP0x1000f000、APMIXED0x10018000。SPM0x10006000は含まれない。
したがって1014のWindows hookは既存INFRA mapのみを使用する。
追加map、GPUCore/MFG map、IRQ、SPM/CPU4/watchdog/clock/PLL writeは無い。
既存parent mutexを操作全体で保持し、Acquireが実WDF D0 leaseを取得。
stop/revoke/timeoutで保護を勝手に解除せず、曖昧なleaseは保持する。

kernel QUERY_INTERFACE `A04SharedGpuPlatformGuid`でAcquire/Protect/
Unprotect/Releaseを提供し、同GUIDのdevice interfaceは発見だけに使う。
GPUのcommand IOCTL、raw-register転送、別GPU PDO、IRQ接続ではない。
consumerは正規parentへのqueryで得たINTERFACE referenceとD0 leaseを保持する。
profileはINFRA_ONLY。SPM操作もGEO DMA_STOPPED/capsも公開しない。

INFRA0x220のmask0x24だけをRMWし、0x228の同maskのackをboundedで待つ。
0x204はway bit7の既存onを読むだけ。Windows hookは0x204へ書かない。
他consumerのbitを保存し、all-ones/error/timeout/古いtokenは失敗する。
保護ackだけをDMA停止証拠と解釈することは不可。

## 純moduleの完全domain sequence

`gpu_domain_owner.c`はSPMを実割当から供給できる場合のoff/onも実装する。
`has_spm=0`ではoff/onはwrite前にUNASSIGNED。現在のWindows hookは必ず0。
必要なSPM範囲は0x214(MFG)、0x2c0(MFG_2D)、0x2c4(ASYNC)、
0x60c/0x610(両status)。SRAM PDN[11:8]/ACK[15:12]、ISO/reset/clock-disable/
ON/ON2だけを各domainの固定offsetで更新する。CPU/WDのoffsetを触らない。
MT8167 status maskはbit4/24/25でMT8173のbit22/23ではない。

off: MFG SRAM ack→isolation/reset/clock-disable→両power ack0→
INFRA0x24 protect+ack→MFG_2D off→SI1 way bit7 clear→ASYNC off。
on: ASYNC on両ack→way bit7 set→MFG_2D SRAM ack0→protect clear+ack0→MFG on。
TOP/PLL/PMIC/MFG内部clockはこのmoduleでは変更せず、正規ownerのclock leaseが必要。
純moduleのDOMAIN_OFF観測も、FW/cache/IRQ/tokenのGEO全proofの代替ではない。

## 一次sourceと判定範囲

- board GPL kernel4.4 `drivers/soc/mediatek/mtk-scpsys-mt8167.c`:
  MT8167 domain table、power_off/on、MFG_2D mask0x24/way bit7。
- 同 `mtk-infracfg.c`: PROTECTEN0x220/PROTECTSTA1 0x228/SI1_CTL0x204。
- XNU f5d43bf1677955e7e367545b9bbbd6449776cdda
  `iokit/Kernel/arm/rogue/A04BR3RoguePower.h`: 同boardのon sequence。
- kernel4.14 eb47f42e1f84fe5716ef239620725f82718a620a、m1.11ED5425693
  `rgxpower.c/RGXPrePowerState`: off ack＋IRQ count drain＋RGXStop。
  `rgxstartstop.c/RGXStop`: MMU/external read/SLC idle＋MIPS Garten idle。
  `rgx_mtk/mt8167/mtk_mfgsys.c`: runtime put MFG→2D→ASYNC。

sourceはack単独を全DMA停止としていない。一方でSPM collapseがあらゆる
powered-idle quiesceに絶対必要だとする一般証明も得ていない。
既存sourceで観測できる完全platform-off pathを純moduleへ実装したが、
現在割当の外へSPMを追加mapして成立したことにはしない。
最終GEO proofはproviderのFW/IRQ/RGXStop・実platform観測・rundown・epochを
結合してから評価する。未確認のstop capsを成功公開しない。

## Optional assigned-SPM profile

The separate machine-only UEFI branch `work/windows-gpu-domain-owner` adds
`A04BR3_GPU_DOMAIN_OWNER=1` (default OFF). It adds SPM to SHR0's actual PnP
resources and preserves the shared ACPI storage GUID. The existing default
profile and GPU0 core/MFG/IRQ assignments are unchanged.

`build-gpu-parent1014.ps1 -WithSpm` chooses gpu-parent1014-spm-profile.json.
It preserves ecaa1008 flags plus GPU flags, requires the explicit sixth raw/
translated exclusive SPM window before mapping, and publishes the separate
V2 A04SharedGpuDomainGuid. The original V1 INFRA interface is unchanged.
Acquire/Observe/Off/On/VerifyOff/ReleaseOff/ReleaseOn all use the same parent
mutex and actual D0 lease. Observe includes TOP snapshots, never clock writes.
ReleaseOff keeps hardware OFF and permits a later acquire of a fully observed
OFF state; it never restarts stale firmware while freeing native resources.

PWRON_CONFIG_EN bit0 must already be enabled by firmware. It is read only.
No shared unlock/key, CPU4/watchdog/wakeup bit is written. The MMIO wrapper
rejects any write outside three fixed GPU power offsets or INFRA masks0x24/0x80.
VerifyOff re-reads all three controls, BOTH power status registers, protection
request/ack and SI1 way, rather than trusting software flags. It still does
not claim native FW/IRQ/cache/epoch rundown or advertise GEO proof by itself.
This full vendor platform-off route is a conservative alternative while the
GPU0-only external write-response contract is unresolved; it is not evidence
that every possible powered-reset solution requires SPM collapse.

## 現行SD-on1010を保持した最終候補（上記ecaa-only記載を更新）

基点9e07f0a6cdf20f6efe9a4a75ed5f42fcd6ee583cをmergeし、実署名済み
`windows-rt-sd-parent1010-sd3-9e07f0a-signed/parent/parent-manifest.json`の
全flagsをprofileへ固定した。A04_SHARED_SD_POWER=1、全audio/WLAN/USB/button/touch flags、
ecaaの実AFE DMA forwardingとSD power hook/sourceを保持する。GPU flagsだけを追加。

専用INF `prototype/shared-parent-engine-pcm-gpu-sd.inf` は
DriverVer=11/07/2026,2026.11.7.1014。通常EnginePcm1008/SD1010 INFを上書きしない。
最終native factoryとの対は `build-gpu-parent1014.ps1 -WithSpm` のV2のみ。
INFRA_ONLYのlibrary/link成功を最終候補として扱わない。
機種UEFIは53d6 SD-onlyを保持する別GPUdomain branchでSPM第六resourceを正規割当する。
共通EDK2、GPU0/MFG/IRQ割当、CPU4/watchdog共有bitは変更しない。

同版正常DeviceDestroy→FW OFF/IRQ count→RGXStop→MTK platform-off→IRQ/MISR rundown→
FW freeを所有GPU/leaseの運用条件とする。全SoC AXI形式証明は追加の無期限前提にしない。
parent単独でnative GPU_DONE/NoDMA proofを返さず、native ownerで正常経路全体を結合する。
