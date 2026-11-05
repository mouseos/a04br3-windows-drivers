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
`sts8167_tb_n.dts` has conflicting-looking VBUS references (GPIO20 pinctrl,
GPIO68 property); board wiring and selection are unverified. XNU's charger
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
