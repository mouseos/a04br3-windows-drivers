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
