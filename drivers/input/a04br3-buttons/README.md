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

`button_pipeline` tracks each button independently using three consecutive
valid samples, and proposes one notification at a time. Failed source reads
pause that source until fresh samples stabilize; successful notifications must
be acknowledged explicitly. An endpoint disconnect invalidates synchronization
and stops notifications until its state is established again. The caller must
serialize sampling, writes and acknowledgements. This component does not read
registers, open the Windows interface, provide a synchronization mechanism, or
guarantee detection of a complete press between samples.

`pmic_read` is a bounded, read-only WACS2 transaction for MT6392 CHRSTATUS
(0x142), using the MT8167/MT8516 wrapper offsets. Register descriptions are
cross-checked against Linux `mtk-pmic-wrap.c`, `mtk-pmic-keys.c`, and the
MT6392 register header in the board's Linux sources; it does not copy Linux
code. Its caller must own and serialize WACS2, provide ordered MMIO and a
delay appropriate to its execution context, and recover hardware after a
timeout before clearing the poison flag. It does not claim resources, handle
interrupts, power or wake, or establish that Windows can share the wrapper.
Host tests simulate the state sequence; actual PMIC access is not verified.

`button_device` connects input callbacks to the state pipeline and a byte
notification callback. The owner must establish the inbox endpoint state
before synchronization and serialize all callbacks. A confirmed write
advances state; a definitely undelivered write can be retried; an ambiguous
result invalidates synchronization to avoid double toggles. It does not
implement a Windows device, hardware resources, interrupts, or wake.

The Windows RT 8.1 inbox driver's read handler statically appears to return
five state bytes; its event queue can update those bytes later and can also
produce the value 2. Do not blindly convert a read to a synchronized mask or
assume a successful notification has already been consumed. Quiescence and
the meaning of non-binary states need runtime verification before integration.
