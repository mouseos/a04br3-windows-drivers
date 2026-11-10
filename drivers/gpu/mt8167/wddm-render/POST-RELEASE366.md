# POST release caller evidence

Captured365 run193 proved that SystemDevice/SystemContext creation succeeds and
that POST quiesce brackets the firmware retirement. It did not identify why the
OS requested POST release. Successful AcquirePostDisplayOwnership information
also means the GPU adapter was the current OS POST owner at acquisition; a
vendor-only GOP DevicePath alone does not establish a recognition failure.

366 records the direct callback return site and at most16 RTL stack frames.
Optional RtlCaptureStackBackTrace/RtlPcToFileHeader/RtlImageNtHeader helpers are
resolved once through MmGetSystemRoutineAddress. Missing helpers or failed
unwind/normalization remain unknown. The display callback keeps its actual
result and normal ownership transitions; no timer, MMIO or capability changes.

The fixed320-byte schema1 record stores FILETIME, interrupt-time, lookup mask,
frame count and normalized-frame mask, followed by17 records of RVA, PE image
size, TimeDateStamp and CheckSum. Slot0 is the direct return site, other slots
are captured RTL frames. Absolute PCs and module bases are never persisted.
Match each normalized frame to captured PE metadata before using its RVA in
REA. Metadata alone does not establish a byte hash; retain the captured PE hash
and exact installed image provenance. Capture count never proves complete unwind.

Store: GE8300Render/Parameters/PostReleaseStack366/StackV1. The read-only native
reader exposes gpu_init.post_release_stack366. It must be joined to fresh R86,
R36 and the actual loaded module within the same captured boot. No extra forced
registry flush is used. Source, build and actual hardware evidence remain
separate; caller capture itself is not successful graphics rendering.
