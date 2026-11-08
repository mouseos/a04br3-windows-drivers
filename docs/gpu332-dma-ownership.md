> Historical packet-DMA experiment only. Current preferred source: gpu332-wddm-aperture.md.

# Explicit DMA resource ownership for the native MDL target

This independent change starts at the exact332 runtime (7c6ef6a8; docsHEAD e41c7321).
It does not include UMD335, claimed-IRQ changes, new GPU capabilities or device trials.

CPU bind now retains only the physical-page lease. PFN/CPU-identity accessors are
for isolation checks only; they never authorize PTE construction or CPU data access.
Before GPU publication, every RT/VB/linear backing obtains real V3 DMA resources,
then the complete PTE union is reconstructed from those actual mappings. CPU
accessors refuse while DMA-owned. Existing direct32 PFN equality checks remain.

The implementation decomposes GetScatterGatherListEx into the documented V3
AllocateAdapterChannelEx(DMA_SYNCHRONOUS_CALLBACK) + MapTransferEx operations.
Each retained64KiB allocation has its own aligned transfer context and public
MapRegisterBase. GetSGEx does not expose this handle; SG.Reserved is opaque and
is never inspected. Explicit handles are necessary for FlushAdapterBuffersEx.
After MapTransferEx, FreeAdapterObject(DeallocateObjectKeepRegisters) frees the
channel before another chunk is requested, while its registers remain retained.
A resource shortage is returned synchronously, with earlier unexposed mappings
flushed/freed. No queued callback/event wait or stack callback context exists.
This is a deliberate API decomposition, not a claim that GetSGEx returns a
flush handle. FreeMapRegisters executes at DISPATCH_LEVEL; all allocations,
MapTransferEx/flush work and page-lease callbacks remain PASSIVE.

After real TA/3D/cleanup completion and SLC+GP synchronization, all MDL PTEs
are zeroed and read back. The retained render context receives a typed m1.11
MMUCACHE command with the existing non-VIVT PT/PD/PC/TLB flags. Both queue drain
and the exact16bit sync update must be observed before any mapping is detached,
flushed or released. A timeout/write/readback/flush ambiguity retains resources,
poisons admission and never reports successful CPU visibility. The MDL lease
survives CPU ownership; next GPU use acquires new mappings and rebuilds every
PTE. Native first-boot and subsequent graph rebuilds use the same transaction.
The legacy native multi-frame path explicitly reacquires/rebuilds/invalidates
its PT before the next frame; it does not retain stale DMA addresses.

When the real parent VerifyOff/IRQ-drain proof is obtained, retirement returns
all RT/VB/linear mappings through the same adapter flush path. Input CPU checks
now validate actual CPU ownership. Shared storage accessors refuse a DMA-owned
CPU mapping instead of dereferencing NULL or bypassing ownership.

The same opaque native ABI/UMD wire contract is retained. These are internal
resource lifetime changes, not fabricated completion or a public feature bump.
A build/sign result is integration-only: no CAT/INF/package/DriverVer or media
is generated, and no silicon MMU acknowledgement or reset resolution is claimed.

Primary contracts:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pallocate_adapter_channel_ex
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pmap_transfer_ex
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pflush_adapter_buffers_ex
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pfree_adapter_object
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pfree_map_registers

The FW command shape/flags reuse the platform's existing typed m1.11
windows_shared_publish implementation and its stock rgxmem.c provenance.
Runtime execution remains gated by its real acknowledgement, not a fixed delay.
