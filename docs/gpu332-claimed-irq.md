# Claimed IRQ failure must remain handled

The native in-process provider returns STATUS_NOT_FOUND without an owned source.
Its other error paths follow a real a04_native_irq_claim, including a failed
receipt and a completed-event identity mismatch. GeoOwnerInterrupt previously
poisoned the engine on these errors before setting handled, so its first claimed
fault returned FALSE to Dxgkrnl. Preserve handled=TRUE after the no-source check.
The failure still poisons/retains the job and reports no completed/preempted fence.

The regression extracts the actual function: the old source fails the claimed
fault assertion; the fixed source passes no-source, claimed receipt error,
claimed identity error, pending work, completed event, and wrong IRQL cases.
No device or DXG runtime behavior is simulated as a functional success.

This corrects shared-ISR ownership reporting, not the unproven run135 LockCb
root cause. It does not implement DMA_FAULTED, manufacture a fence, bypass
cache/MDL ownership, or alter UMD/native runtime code. Integration artifact only:
one KMD lifecycle TU, all other KMD objects/native/UMD retained. No new package,
CAT, DriverVer or deployment media is generated.

Source contract:
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkddi_interrupt_routine
