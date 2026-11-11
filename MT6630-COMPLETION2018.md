# MT6630 BthMini READ completion reentry

The inbox RT8.1 BthMini.SYS (SHA256
18be9f55df80e137e77cea47cca8e210977ac1f82436bb2e6c9d64777cbb654b)
resubmits asynchronous HCI READs inline from completion FUN_000127f4 through
FUN_00012580. It also calls BTHPORT_RecvMpBip inline on a valid event.
This was checked with REA; it is shipped-artifact evidence, not original source.

The old client deliver() completed a request with the nonrecursive client
WDFWAITLOCK held. An inline new READ enters ComboInternal and takes that same
lock. Timer -> deliver -> complete -> BthMini -> READ -> lock cannot finish;
D0Exit also needs the lock. This is a concrete reentry defect, but the specific
run167/run169 9F blocked IRP/thread has not been attributed to it.

Detach and copy the packet while locked, remove it before completion, unlock
around WdfRequestCompleteWithInformation, then reacquire and rescan. No packet
pointer or old index survives completion. Failed output-buffer requests complete
unlocked without consuming the packet. Limit each delivery pass to32 completions.
The caller still owns the lock on return. No HCI response, radio interface,
DMA drain or power completion is fabricated. Provider1026 and broker ABI retain
all existing contracts. Only client combo_client.c is recompiled.

The single ASAN/UBSAN test extracts actual deliver() and exercises inline READ
reentry, mixed event/ACL queues, failed-buffer retry, packet detach and exactly
once completion. Queue calls are mocks; this does not emulate HCI or prove RF,
radio publication or a hardware power fix. Test output must be inside task root.
