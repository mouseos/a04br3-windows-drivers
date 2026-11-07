# MT8167 USB 2.0 EP0 sequencing

This is a partial host control-transfer engine and EP0 PIO adapter, not an
installable Windows driver. It sequences setup, data and status, short IN
packets and errors. The caller must serialize EP0, perform FIFO and CSR I/O,
validate hardware completion and errors before calling Ep0Advance, manage
retries/deadlines, and recover the controller after abort. Never call it on an
unverified interrupt. This code does not drive VBUS or touch MMIO.

The GPL Android source `arch/arm64/boot/dts/mediatek/mt8167.dtsi` identifies
USB0 as `mediatek,mt8167-usb20`, controller 0x11100000, PHY range
0x11110000, GIC SPI 72, and USB1 as a separate `mt8167-usb11` at 0x11190000.
The vendor USB20 driver is MUSB, not an EHCI or xHCI register interface.
`a04br3.dts` specifies GPIO22 for VBUS drive, GPIO41 ID, GPIO21 overcurrent, GPIO34 USB detection, and the MT6392 VUSB regulator. The earlier `sts8167_tb_n.dts` GPIO20/GPIO68 values belong to a different board and must not be used here. Physical wiring and charger ownership remain unverified. XNU's charger
code does not establish a USB host driver. Do not enable VBUS based on these
references alone.

Windows 8.1's host stack supports standard EHCI/xHCI; the current Microsoft
UCX documentation must not be assumed to describe availability in 8.1.
The WDK 8.1 headers and target OS exports still require verification before
choosing a host-stack integration. Sources:
https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-3-0-driver-stack-architecture
https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/developing-windows-drivers-for-usb-host-controllers

Missing: ACPI resource contract, clock/PHY/power and VBUS ownership, root hub,
interrupt and FIFO integration, addressing, keyboard interrupt endpoint,
PnP/power lifecycle, cancellation, and Windows USB/HID stack integration.
Neither USB enumeration nor keyboard operation is established.

Run portable tests from repository root:
`cc -std=c11 -Wall -Wextra -Werror -Idrivers/usb/mt8167-usb20 drivers/usb/mt8167-usb20/ep0.c tests/usb/ep0_test.c -o <output-in-workspace> && <output-in-workspace>`.
`tools/ci/build-mt8167-usb20.ps1` compiles the engine with WDK 8.1 headers for
ARM32 and verifies the COFF machine; this is not a linked driver package.

The EP0 PIO adapter assumes the indexed register layout and an initialized host-mode controller. It performs FIFO I/O and CSR0 transactions through callbacks, but does not install an IRQ handler or manage power, route, cancellation or recovery. Mock tests are not hardware validation.

The boot-keyboard-only path in keyboard.c parses the first configuration and selects a direct boot keyboard interrupt IN endpoint; keyboard_rx.c implements MUSB PIO endpoint operations. keyboard_session.c connects EP0 enumeration and RX PIO to a report callback, and wdf_keyboard.c connects that callback to a KMDF HID transport. The board provider, PnP resources, actual IRQ delivery and power/PHY callbacks are not connected, so this is not a usable device driver. No hubs, general USB devices or arbitrary HID report protocols are supported.

The HID transport is linked as an unsigned diagnostic .sys only; no INF or ACPI device is provided. It is not an installable or hardware-ready package. A board owner must control GPIO22, regulator/clocks, overcurrent and BQ24160/I2C2 ownership and perform PHY setup, FIFO allocation, reset, timeouts and safe teardown before invoking the session.

## PnP, IRQ and shared power (prototype)

`wdf_controller.c` validates and maps the two 64 KiB ACPI memory resources
and requires one interrupt, captures enabled MT8167 L1/TX/RX/USB causes in
the ISR, and defers HID work to PASSIVE_LEVEL. `controller.c` configures EP0
and RX1 FIFO regions and masks/acknowledges the corresponding interrupts.
`gpio22.c` performs only GPIO22 SET/CLR operations after checking GPIO mode;
it must be called by a provider holding the pin and register ownership.

`KbHidActivate` consumes callbacks from an exclusive board-power owner; no
such provider or ACPI wiring exists here, and nothing calls Activate. The
BQ24160/I2C2 and MT6392/clock lifetimes remain unresolved. Consequently IRQ
arming, GPIO control and enumeration cannot be exercised by this package,
regardless of successful compilation. Do not install or use on hardware.

The later shared-parent prototype supplies a USB ownership interface and a
boot-keyboard INF, and connects the child D0 callback to that interface.
It checks the Linux-derived MT8167 clock configuration, GPIO overcurrent and
MT6392 VUSB before attempting VBUS. The parent D0 entry still deliberately
rejects startup: existing direct owners have not been migrated, interrupt
polarity and the GPIO/rail ownership are unverified, and BQ24160 coordination
is incomplete. Portable owner and resource tests pass. The keyboard,
controller and shared-parent translation units compiled with WDK 8.1 ARM32
headers. The keyboard-side SYS also linked against the WDK ARM libraries;
the shared parent was only compiled. The INF and complete package have not
been validated or tested on the target. It is not an installable or working
USB host driver.

The keyboard controller now validates the paired raw and translated PnP
resources before mapping: only the board's raw MAC/PHY windows, two distinct
exclusive translated windows and the exclusive level-sensitive GSIV 104
interrupt are accepted. The translated addresses, which need not equal the raw
ones, are used for MMIO. A malformed or overlapping assignment is rejected
before mapping. This does not validate the physical IRQ route, PHY sequence,
VBUS, connector, or operation on the tablet.

If the shared owner's overcurrent check fails while enabling VBUS, the owner
now attempts VBUS-off immediately and latches the fault; VBUS-off and release
remain available for a faulted lease. A release that cannot verify GPIO22 low
retains the lease. The parent retains mappings for an unresolved lease and
never retries GPIO MMIO after surprise removal; this software retention does
not guarantee that Windows PnP retains physical resources. A failed host VBUS
start now also executes the existing session/PHY/VBUS shutdown sequence.
These failure paths are host-tested only; real overcurrent polarity and VBUS
switch behavior remain unverified.

The controller's diagnostic IRQ arm now masks its L1 sources, disables the
endpoint enables and clears their pending W1C status before enabling them.
Readback failure disables the sources again. WDF serializes arm and disarm
against the ISR. The board Linux host also calls `tbl_charger_otg_vbus` in
addition to switching GPIO22; the current parent does not own or validate that
charger OTG path, so this code does not establish usable VBUS. GPIO, PHY and
IRQ behavior remain unverified on the device.

VBUS-on now requires a serialized OTG/source acquire and release pair from
its actual owner before GPIO22 can go high. The switch is driven low before
source release; failed or uncertain acquisition/release retains and poisons
the lease. No real source provider is registered yet, so VBUS-on is rejected.
The BQ24160 charger owner currently supports watchdog-only writes and lacks a
qualified firmware handoff and a cross-driver OTG interface. TI specifies that
register 0x01 OTG_LOCK blocks USB-input charging while an external OTG supply
is present; it is not a command to generate 5 V. The board's external source
and BQ handoff must be established before wiring a provider. The callbacks run
under the shared-parent mutex and a future implementation must define lock
ordering and watchdog lifetime without reentering the parent.
Sources: [TI BQ24160 datasheet](https://www.ti.com/lit/ds/symlink/bq24160.pdf),
[Linux USB host](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/drivers/misc/mediatek/usb20/mt8167/usb20_host.c).

## Guarded Windows source and session ordering

The shared-parent WDF source can now register a board-specific GPIO22 source
provider with `A04_USB_GPIO22_SOURCE_EXPERIMENT=1` (default 0). It requires
the experimental shared query, child topology, button D0 and APMIXED resource.
The provider checks ID41 low and USB-detect34 inactive before permitting the
existing owner to raise GPIO22; health polls also check ID and overcurrent,
and faults stop the MUSB session. The off path lowers GPIO22 before releasing
the source lease. It makes no BQ24160 writes: the a04br3 Linux defconfig selects
BQ24160 and the vendor `tbl_charger_otg_vbus` has no BQ24160 branch. GPIO22
being an actual 5 V source enable, GPIO34 electrical behavior, ID/overcurrent
polarity, pinmux and protection are not established by these software facts.
The experiment is not authorized by detecting the flags alone and must remain
disabled until the board wiring and exclusive ownership are established.

The HID diagnostic now prepares clocks/PHY, arms and clears IRQs, and only
then starts VBUS and the MUSB session; previously arm cleared CONNECT after
the device could already have connected. The lock keeps queued IRQ work from
starting enumeration before startup completes. This remains a direct boot
keyboard diagnostic, not a general Windows USB host controller/root hub.
The former implementation rejected D0 for absent host cable/external VBUS.
The run27 correction below adds admission retry;
actual PnP binding, signed package, IRQ delivery, electrical VBUS and keyboard
operation remain unverified. `make test-mt8167-usb-source` exercises the actual
parent callbacks with stub MMIO, not hardware.

If GPIO22 enable readback fails, the owner now attempts an immediate verified
switch-off before releasing the source. An unverified switch-off or source
release retains the lease and poisons the owner. This is a software fault
path test, not a measurement of connector voltage.

The diagnostic uses PIO, not DMA. Its PnP callbacks, resource validation and
ISR/DPC exist, but the parent deliberately rejects D0 and actual ACPI binding,
interrupt routing and safe power ownership are not established. A general
Windows USB host still requires a supported Windows 8.1 host-stack interface,
root hub and device PnP, transfer scheduling and cancellation, and a verified
DMA design if DMA is used. The XNU charger implementation observes the BQ24160
and its board handoff; it does not establish the connector's 5 V source.

## Experimental host core (default off)

`host_core.c` models one root port and a bounded serialized transfer queue.
Its power indication requires a caller's verified VBUS result; it does not
acquire power, drive registers or bind to Windows. Connect, disconnect, reset
completion and overcurrent are reported by an eventual hardware adapter. An
active request cannot finish cancellation or disconnect until that adapter
confirms endpoint and DMA quiescence. Callers serialize all operations and
must complete queued requests when offline. `make test-mt8167-usb-host-core`
tests these contracts. Set `A04_MUSB_HOST_CORE_EXPERIMENT=1` for the ARM WDK
compile script to compile the core as a separate object; it is not linked into
the diagnostic driver. Linux `musb_virthub.c` supplies the reference for the
single port, reset and change bits; `musb_host.c` documents the distinction
between queued and hardware-active cancellation.

The [Microsoft UCX controller creation DDI](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ucxcontroller/nf-ucxcontroller-ucxcontrollercreate)
lists Windows 10 as its minimum client; the available 8.1 WDK lacks its UCX
headers. Do not infer a Windows RT 8.1 host stack integration from this model.
PnP enumeration, root-hub requests and notifications, endpoint scheduling,
timeouts, hardware execution, DMA and Windows request completion remain to be
implemented against a verified 8.1 interface. Neither hardware nor VBUS has
been validated by these tests.

### Windows 8.1 interface evidence and EP0 PIO adapter

Microsoft describes USBPORT plus the Microsoft-supplied EHCI/OHCI/UHCI
miniports in the [USB host stack](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-3-0-driver-stack-architecture), and its
[debugger documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/-usbkd-usbhcdlist)
shows an internal registration packet. In the locally available WDK 8.1 kit,
no USBPORT miniport registration header or import library was found. This is
not proof that Microsoft has no private or partner-only interface. The kit's
`usbbusif.h` describes `USB_BUS_INTERFACE_USBDI` as a function-driver
interface; [Microsoft](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/usbbusif/ns-usbbusif-_usb_bus_interface_usbdi_v3)
says it is provided to USB clients by a hub, so it cannot register a MUSB
controller. A partner DDI or a complete independently implemented host and
hub bus remains unverified. [UCX](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ucxcontroller/nf-ucxcontroller-ucxcontrollercreate)
and [UDE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/udecxwdfdevice/nf-udecxwdfdevice-udecxinitializewdfdeviceinit)
list Windows 10 as their minimum. A [HID transport minidriver](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/transport-minidrivers)
is an applicable class-specific alternative, not a general USB host; the
existing keyboard diagnostic explores that path and remains electrically gated.

`host_control.c` connects the experimental serialized queue to the existing
MUSB EP0 PIO engine and accepts acknowledged events from
`Mt8167UsbInterruptCapture` or the WDF interrupt worker. It supports direct
control transfers only, with no hubs or DMA. Disconnect and VBUS errors win
over a simultaneous EP0 event; a VBUS error is not assumed to be a measured
overcurrent. Cancellation and errors require the caller to stop the endpoint
and drain stale interrupts before acknowledging quiescence. Port power, bus
reset timing, queue serialization, lifetime, IRQ draining, deadlines and all
Windows request completion remain the caller's responsibility. Nothing creates
or binds this adapter in the current driver and the existing VBUS gate is
unchanged. `make test-mt8167-usb-host-control` injects events through the
real interrupt capture and mock MMIO; it does not exercise physical hardware.

### WDF HID control experiment

With both `A04_MUSB_HOST_CORE_EXPERIMENT=1` and
`A04_MUSB_HID_CONTROL_EXPERIMENT=1`, the existing direct boot-keyboard WDF
transport accepts `IOCTL_HID_GET_INPUT_REPORT` after enumeration and issues
one HID GET_REPORT(Input, ID 0) over EP0. The request remains in a WDF manual
queue while EP0 uses an eight-byte device-context buffer. WDF handles queued
request cancellation; the hardware transfer is not immediately aborted when
that happens, and the context buffer cannot be reused until completion. A
2-second deadline rejects further EP0 use after a timeout. Disconnect,
power-health failure or EP0 error also poisons the experimental path when a
transfer is active. IRQs are disarmed under the interrupt lock, queued DPCs and
work are flushed, and queued requests are purged before MMIO release. Do not
flush the work item from its own callback or while holding its controller lock.

This diagnostic remains gated by the existing shared power owner and cannot
establish connector power or provide general USB PnP. Neither WDF cancellation
races nor the interrupt drain have been exercised under Windows Driver Verifier
or on the device; ARM32 compilation/linking and host MMIO simulations do not
establish those results. No new signed or installable package is produced.

### キーボードLED出力

`A04_MUSB_HID_CONTROL_EXPERIMENT=1` と既存ホストコア実験の両方を
有効にした構成で、`IOCTL_HID_WRITE_REPORT` と
`IOCTL_HID_SET_OUTPUT_REPORT` を既存EP0へ接続する。記述子にNum Lock、
Caps Lock、Scroll Lock、Compose、Kanaの5bitと3bitの定数paddingを追加し、
入力の8byte形式は維持する。既定OFF構成は従来の入力専用記述子を返す。

要求は列挙完了後に1件だけ受け付け、既存GET_INPUT_REPORTと同じロック・
手動キュー・2秒期限を使用する。ID 0の1byteレポート、または先頭に0を
付けた2byte形式だけを受け付け、USBへはLEDの1byteだけを送る。
非0のID、paddingの設定、不正な長さを拒否し、送信用バッファへコピーして
からWDFへ要求を渡す。GET_INPUT_REPORTも8byteとID 0付き9byteの両方を
検証し、バッファの範囲内へコピーする。実際のWindows 8.1 HIDClassから
渡される形式とキャンセル競合は、このホスト試験では確認していない。

USB SET_REPORTの値は、端末GPLソースの
`kernel-4.4/drivers/hid/usbhid/usbkbd.c` のLED初期化にある
`bmRequestType=0x21`、`bRequest=9`、`wValue=0x0200`、
`wIndex=bInterfaceNumber`、`wLength=1` に基づく。
[MicrosoftのHID_XFER_PACKET](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/hidclass/ns-hidclass-_hid_xfer_packet)、
[IOCTL_HID_WRITE_REPORT](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/hidport/ni-hidport-ioctl_hid_write_report)、
[IOCTL_HID_SET_OUTPUT_REPORT](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/hidclass/ni-hidclass-ioctl_hid_set_output_report)
を参照。完了情報はUSBへ転送した1byte。ステータス段階まで完了してから
成功を返し、STALL・切断・期限切れは既存の失敗経路へ渡す。

`make test-mt8167-usb-keyboard-output` は全32値を実際のホストキューと
EP0 PIOへ通し、SETUP、データ、ステータスの完了順序、IDの除去、
不正要求、STALL、記述子の入力64bit・出力8bitを確認する。
MMIOの模擬試験であり、WindowsでのWDF実行や実機LED点灯の確認ではない。
給電のゲートは変更しない。汎用USBホスト、hub、電源・IRQの実機検証は
引き続き未完了で、動作済みパッケージとして配布できない。

### USB専用PnP構成と入力経路の継続実装

`c027fa3`からの継続では、[KEYBOARD_INTEGRATION.md](KEYBOARD_INTEGRATION.md)の
共有親＋USB PDO＋HID transport構成を追加した。本文中の古い未接続説明は
当時の記録であり、同文書の接続表を現在のソース状態として参照する。
実機、SSH、WDKビルドの実行は主担当の工程として残る。

通常OSの既存入力・batteryを維持する構成は、同文書の「通常OSを維持する統合profile」を参照。
`build-mt8167-usb-keyboard.ps1`の既定はIntegratedへ変更し、08faae5との
単一parent＋audio＋USBは`build-mt8167-first-playback.ps1 -IntegratedUsb`で生成する。


## run27: device-role待機と開始失敗の区別

run27のSetupAPIはMT8167\USB20の登録成功とCode10を記録するが、
Prepare/D0/親Acquireのどこで失敗したかは記録しない。System.evtxの
UserPnp 20001もInstallStatus=0で、PCケーブルが原因だったとは確定できない。
一方、従来コードが親の正常なhost admission拒否をすべて
STATUS_DEVICE_POWER_FAILUREへ潰していたことはソースで確認できる。

機種Linux一次ソースは上記固定commitのusb20_host.c:114–116でGPIO34の
lowをUSB検出、同:245–278でID highをdevice-roleとする。
a04br3.dts:686–691/751–752とmt8167.dtsi:944はID41、VBUS22、detect34を指定する。
この修正は既存の拒否条件を変更せず、親のread-only preflightから
1=device-role、2=external VBUSという副作用なしの待機だけを返す。
VUSB/clock/GPIO22/PHY/MAC/leaseには触れず、poisonもしない。
不明な正値・負値、clock/PMIC/configuration失敗は失敗のままにする。

実KbD0はこの2理由だけをinputなしの正常D0として扱う。PASSIVE_LEVELの
既存one-shot timerで500msごとに再admissionし、host資格を得たら既存の
Prepare→IRQ arm→HostRunを通す。controller_lockを解放してからActivateを
呼ぶため再入しない。D0Exit/Releaseは従来のWdfTimerStop(TRUE)でcallbackを
joinしてからIRQ/interface/mappingを解放する。待機後の実失敗はfaultで停止。
新しい失敗を以前の待機値で隠さないよう、各Activateの最初に理由を消す。
実PnP startの成功だけをUSB列挙・給電・keyboard入力成功と扱わない。

DbgPrintはadmission理由の変更時のみ出し、resource/query-interface/PIO-DMA拒否/
PHY prepare/IRQ arm/HostRunを分けて失敗を出す。UARTやEVTXへ自動保存される
保証はないため、主担当の既存収集経路で確認する。active hostからケーブルを
抜いた際のhealth faultは従来どおり停止し、全面的なdual-role切替は実装しない。

共有interface v1のlayout/GUIDは不変だが、戻り値の意味を拡張するため
**更新した単一統合親とUSB SYSを対でbuild/適用する**。
両INFはDriverVer=11/06/2026,2026.11.6.1002へ更新した。
run27のUSB 2026.11.6.1001より新しい版であり、包装時に旧版で上書きしない。
ユーザーのrun28報告では親/WiFiは旧SYSで、INF更新だけで適用済みとは扱わない。
EnginePCM親de214 newcacheも未適用との追加報告があり、旧親のCode10を
新親の結果として扱わない。新親の機能と既存flagを保持してから対で比較する。
主担当の最新統合ソースへこのcommitだけを取り込み、audio/WiFi/入力/電池等を
含む単一BEN0001 SYSを維持する。このbranchの親SYSで丸ごと置換しない。

build recipeはtools/ci/build-mt8167-usb-keyboard.ps1 -Profile Integrated。
既存audio/WiFi等の環境flagを保持した主担当の固定WDKで実行し、親子とも新INFで
CAT生成/署名する。ホスト確認はmake test-mt8167-usb-keyboard-batch
 test-mt8167-usb-integrated。read-only preflight、待機理由、未知値拒否、lease/poison/
MMIO副作用なし、host再取得、単一USB/audio親の既存試験を確認する。
WDK compile/link・新CAT・実PnP D0・電圧・入力・抜差しは未確認。

## USB1003開始失敗の切り分けと1004候補

run33の回収済み`service.log`（RUN F6F2）では、親1006のProblem=0に対し、
USB1003はCode10・ProblemStatus=`C000009C`（`STATUS_DEVICE_DATA_ERROR`）。
親正常・SYS hash一致だけでは、USBのPrepare/D0やHID初期化成功を証明しない。
`KbPrepare`/`KbD0`の資源・電源失敗の直接戻り値はこの値と異なるため、
この観測だけでLinux由来PHY/VBUS手順を変更する根拠はない。

1004候補はMicrosoft公式
[vhidmini2](https://github.com/microsoft/Windows-driver-samples/blob/main/hid/vhidmini2/driver/vhidmini.c)
の`RequestCopyFromBuffer`と`GetString`に沿い、HID出力を
`WdfRequestRetrieveOutputMemory`/`WdfMemoryCopyFromBuffer`で処理する。
取得・copy失敗statusはそのまま返し、成功時だけ転送byte数を設定する。
`IOCTL_HID_GET_STRING`には本transportの固定メーカー・実IC製品名と空のserialを返す。
接続キーボードの識別子・入力内容を記録したり、serialを捏造したりしない。
またadmission再試行の終端失敗とport faultは、元statusを記録した上で
`WdfDeviceSetFailed(WdfDeviceFailedNoRestart)`を通知し、自動restartを繰り返さない。
これらは実装上の契約修正であり、旧Code10の原因確定・解消実測ではない。

### 自サービスParametersの固定checkpoint

保存先は`HKLM\SYSTEM\CurrentControlSet\Services\A04Usb20Keyboard\Parameters`のみ。
各値はREG_BINARY、little-endianの32-bit整数8個（32byte）:
`version=1, stage, status(NTSTATUS), detail0, detail1, prior_write_status,
start_low, start_high`。最後の2整数は本device作成時の`KeQuerySystemTime`。
`InitAdd`と同じstartの値だけを今回の記録として扱う。古いbootの残存値・未記録の
checkpointを到達成功と解釈しない。保存失敗は次の記録の`prior_write_status`に残り、
本来のPnP/IOCTL statusを上書きしない。強制registry flushは行わないので、
reset後の記録欠落は未到達の断定材料にしない。

| stage | 値名 | detail0 / detail1 |
|---:|---|---|
| 0 | InitAdd | source line / HID-control有効値 |
| 1 | InitResources | raw / translated resource件数 |
| 2 | InitParent | interface size / version |
| 3 | InitPower | Acquire実戻り値 / 0 |
| 4 | InitPio | DMA停止確認実戻り値 / 0 |
| 5 | InitBoardPrepare | admission実戻り値 / prepare実戻り値 |
| 6 | InitArm | 0 / 0 |
| 7 | InitRun | run実戻り値 / 0 |
| 8 | InitD0 | admission実戻り値 / board-owned |
| 9 | InitAdmissionWait | admission実戻り値 / 0 |
| 10 | InitHidDescriptor | HID descriptor byte数 / report byte数 |
| 11 | InitHidAttributes | attributes byte数 / output byte数 |
| 12 | InitHidReport | report descriptor byte数 / output byte数 |
| 13 | InitHidString | string ID / output byte数 |
| 14 | InitFault | port stage / enumeration stage |
| 15 | InitRetryFailure | admission実戻り値 / 0 |
| 16 | InitHidOther | 最初の未対応IOCTL code / output byte数 |

各stageはdevice lifetimeで最初の1回だけ記録（最大17値）。poll/report毎の書込みや
ISR/DPCからのregistry操作はない。admission待ちでは実activation失敗statusを残し、
意図したケーブル待機のPnP受付と実host開始を区別する。D0再入時や後続の全IOCTLを
網羅するtraceではない。実hardware開始の根拠は`InitPower/Pio/BoardPrepare/Arm/Run`と
実列挙・入力結果を併せて判断する。HID checkpointがすべて成功でもOS側parse成功や
キーボード動作の証拠とは扱わない。

変更scopeはUSB専用source/INF/helperのみ。親1006・既存eMMC・EDK2は変更しない。
helperの既存NoFP/NEON flagsと正規WDK ARM runtime linkを維持する。
固定commitで`A04_MUSB_HOST_CORE_EXPERIMENT=1`と
`A04_MUSB_HID_CONTROL_EXPERIMENT=1`を指定して
`tools/ci/build-mt8167-usb20.ps1 -OutputDirectory <checkout内の専用出力>`を実行する。
実WDK build・署名・Windows再起動/実機確認はこのsource変更では未実施。

## WDF work-item creation

The 1004 runtime checkpoint returned STATUS_WDF_EXECUTION_LEVEL_INVALID
(0xC0200211) from controller creation, before hardware preparation or HID IOCTLs.
WdfWorkItemCreate validates attributes without allowing an ExecutionLevel override.
Keep its initialized InheritFromParent value; EvtWorkItem already runs at PASSIVE_LEVEL.
Keep AutomaticSerialization disabled and the existing explicit controller lock,
IRQ/DPC handoff, work-item flush, and device execution level unchanged.
This corrects object creation; host power, enumeration, and keyboard input still
require hardware verification.

Sources: Microsoft Windows-Driver-Frameworks shared/core/fxworkitemapi.cpp and
shared/object/fxvalidatefunctions.cpp; WDK 8.1 KMDF 1.13 wdfstatus.h;
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfworkitem/nf-wdfworkitem-wdfworkitemcreate

## USB1006: device-role待機とD0実戻り値の分離

run83の同一start記録ではInitParentの32/1はinterface Size/Versionであり、
flags/lease数ではない。InitAdmissionWaitとInitD0のdetail0=1は親Acquireの
A04_USB_WAIT_DEVICE_ROLE。親がGPIO DIN2(0x220)のID41 bit9 highを観測した
というソース上の経路で、Power/Pio/PHY/Arm/Run到達ではない。GPIO34 lowによる
外部VBUS待機は別の理由2。Linux usb20_host.cもID highをdevice-roleと判定する。
この記録から親のRTC/PLL/PMIC失敗、lease取得成功、host給電成功を推論しない。

USB1005はhost admission待機時のKbD0がSTATUS_SUCCESSを返す一方、
InitD0へ内部activationのSTATUS_DEVICE_POWER_FAILUREを保存していた。
USB1006は待機をSTATUS_PENDINGとして表し、InitActivation(stage17)へactivation
実status/admission/board-owned、InitD0へD0の実戻りstatusを分けて保存する。
既知理由1/2だけが無lease待機であり、未知理由・負値・実電源/PHY/IRQ/Run失敗は
従来どおり失敗する。host開始やVBUS成功を合成せず、500ms既存timer再admissionを
維持。schema1/32byte/start一致・一stage一記録を維持し、レジスタ操作は追加しない。

親1026(capture+RTCborrow)のsourceを基点に、実USB1005の既存HID出力/開始checkpoint/
workitem修正を保持したleaf候補。親コード・flags・資源・interface ABIは1026から
変更せず、親SYSはbuild/署名/置換しない。Linux/XNU根拠と未解決の境界は
[ADMISSION-1006.md](ADMISSION-1006.md)を参照。実機未投入、host給電改善は未確認。

## USB1007: interrupt INの待機期限

USB1006のadmission/D0記録修正とUSB1005のHID/workitem修正を保持する。
READYで発行したinterrupt INを、レポートが来ないという理由だけで2秒後に
失敗させる処理を削除した。キーボードの無変化・NAKは故障ではない。既存の
KbRxConfigureが受理するhigh-speed bInterval=16だけでも周期は4096msとなり、
従来の2000ms期限と矛盾していた。Linux MT8167 vendor musb_host.cの
musb_host_rxも、non-iso NAKをエラーとせず要求を継続する。

列挙/EP0の進捗期限、HID controlの期限、STALL/protocol error、不正レポート長、
disconnect、親のhealth検査、device-role/外部VBUS admission guardは変更しない。
IRQ喪失だけをidleと区別して検出する機構は本修正では追加しない。
KB_PORTのメンバー配置を保持し、親1026・MT6392・GPIO22・PLL・PHY sequence・
資源・EDK2・GPUを変更しない。

回帰試験は実keyboard_port/session/EP0を使用し、2000ms、4096ms、300000ms、
32bit時計のwrapをまたぐ無レポート待機後にも入力できること、NAK再試行、
STALLの失敗、切断、列挙の2000ms期限を確認する。旧1006は同fixtureの
2000ms待機で失敗する。ASAN/UBSANと既存USB portable試験を使用する。

これはhost移行後の候補修正であり、run97のID41 device-role待機を解消しない。
run97のInitParent status0/detail0=32/detail1=1はinterface size/version、
InitAdmissionWaitと旧InitD0のstatusC000009E/detail0=1は既知待機理由1である。
parent/resources/HID成功とUSB給電・列挙成功は区別する。既存USB1005を実機で
保持し、本候補の実機投入は主担当の判断に委ねる。

根拠: Linux vendor kernel-4.4/drivers/misc/mediatek/usb20/musb_host.c
(musb_host_rxのMUSB_RXCSR_DATAERROR処理、interrupt bIntervalの速度別encoding)。
機種のusb20_host.c/usb20_phy.cとDTSのID41/GPIO22/detect34/MT6392 VUSBは
既存実装と照合済み。UniXNU-a04br3のA04BR3Charger系はcharger監視であり、
コネクタ5V成立やUSB host稼働を証明するものとしては使用しない。

## USB1008: boot mouseの追加候補

既定OFFの `A04_MUSB_BOOT_MOUSE_EXPERIMENT=1` で、直結1台の標準HID
boot mouse（class3/subclass1/protocol2）を追加する。keyboardとLED、USB1007の
idle待機を保持する。Windows側はID1 keyboard/ID2 mouseの2コレクションとなる。
mouseは3ボタンと相対X/Yのみ。詳細な対応範囲・根拠・ビルド・検証は
[BOOT-MOUSE.md](BOOT-MOUSE.md)を参照。USB1007の署名済み正本は別途保持する。

1008のportable/WDF shimとビルド成功は、実機のhost移行・VBUS給電・列挙・
mouhid/kbdhid経路の動作成功を意味しない。device-roleでの適切な待機を維持し、
roleの強制切替、親1026/MT6392/PHY sequence/UEFI/GPUの変更は行わない。

## USB1009: host MACの開始・継続条件

BDEVICE/SESSIONの期限付き確認と、稼働中MAC role/session喪失時の停止を追加した
候補。詳細は[HOST-CONVERGENCE.md](HOST-CONVERGENCE.md)。USB1007/1008の正本を
保持する。PC接続中のdevice-role待機は解除せず、5V給電・実列挙成功とは扱わない。
親1026・GPIO/PMIC/PLL/PHY書込み列・資源・EDK2・GPUは変更しない。
