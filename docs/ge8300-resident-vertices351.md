# GE8300 resident vertex buffers

GPU350 reached a standard D3D11 red triangle on Windows RT 8.1. The next 96-byte VB failed because the native owner admitted only one resident vertex MDL. The shipped RT 8.1 VidMm rejects STATUS_GRAPHICS_ALLOCATION_BUSY from BuildPagingBuffer with bugcheck 10E/B.

Each VB now owns its input MDL in its resource, like other read-only linear resources. The per-draw vertex pointer borrows that binding after real drain, IRQ closure and platform OFF. The selected VB uses the vertex PT attachment; every other resident buffer stays in the union PT at its VidMm offset. Unbind retires the graph and clears a selected alias before releasing the resource-owned MDL. No fabricated completion, replacement backing or successful paging status is introduced.

Incremental recipe: `tools/ci/build-ge8300-resident-vertices351.py`. The immutable GPU350 package is pinned; only factory, factory runtime, paging wrapper and runtime identity are rebuilt. The other 76 objects, UMD/compiler, firmware and diagnostic client remain fixed. Normal full-source builds use the same source closure and do not depend on a private checkout.

Build and hardware verification remain separate. This source change has not yet been validated on the device. Physical display output, general Windows GPU rendering, texture/gradient and power-transition stability remain incomplete.
