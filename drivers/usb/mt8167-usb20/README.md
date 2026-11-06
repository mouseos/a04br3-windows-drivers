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
An absent host cable or detected external VBUS rejects D0; hotplug retry,
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
