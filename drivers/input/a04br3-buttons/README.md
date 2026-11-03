# A04BR3 button state support

`button_state` is a shared, hardware-independent state translation component.
It does not enumerate a device, read hardware, inject events, or provide wake.

The A04BR3 device tree maps MT8167 keypad MEM1 bits 0 and 1 to Linux keycodes
115 (volume up) and 102 (home).  MT6392 CHRSTATUS bit 1 reports power and bit 2
reports volume down.  All four inputs are active low.  See the device tree and
`drivers/input/keyboard/mtk_pmic_keys.c` and
`drivers/input/keyboard/mediatek/kpd.c` in the supplied Linux 4.4 source.  These
are board and register facts; this implementation does not copy Linux code.

Microsoft documents the [Windows 8.1 button notification interface](https://learn.microsoft.com/en-us/windows-hardware/drivers/gpiobtn/available-interfaces-and-related-apis):
indices 0 through 4 are power, Windows, volume up, volume down and rotation
lock, with each notification toggling the previous state.  The first four are
used here.  Only acknowledge an event after the standard driver accepted it;
initialize `delivered` to zero when that driver is known to have initialized
all buttons released.  If its state becomes unknown, resynchronize explicitly
instead of guessing.  Merely sampling state cannot capture a press and release
between samples; interrupt delivery, suspend and wake require separate design
and device verification.

Run `make test` from the repository root.  The tests do not demonstrate hardware
behavior or Windows integration.
