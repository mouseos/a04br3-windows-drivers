# WDDM1.x physical aperture lifetime (preferred over packet-DMA leases)

The target pages are supplied by VidMm to the original
DXGK_OPERATION_MAP_APERTURE_SEGMENT, not allocated by an arbitrary kernel
caller. Microsoft specifies pMdl as physical system-memory pages and explicitly
uses MmGetMdlPfnArray(pMdl)[MdlOffset] to identify the first page. A linear
aperture KMD programs its own page table, which is not known to VidMm.
The existing frontend validates the exact page offset/extent/allocation, pins
an independent same-PFN span, and retains map_ref plus native_refs. Neither
UNMAP nor an OS MDL lifetime is replaced with a guessed address.

For this RT8.1/WDK8.1 WDDM1.x path, those provided physical pages are the GPU
MMU backing addresses. This is not a claim that arbitrary MmGetPhysicalAddress
or any user MDL is DMA-safe. Original MAP_APERTURE is distinct from later
WDDM3 logical-DMA remapping, whose ADL replaces the old physical MDL. No ADL,
WDDM2/3 IOMMU capability, PASID, arbitrary CPU pointer, common-buffer MDL or
user-controlled physical address is accepted by this route.

MT8167 platform provenance:
- stock sysconfig.c UMAPhysHeapCpuPAddrToDevPAddr (lines108-124) explicitly
  copies CPU physical addresses to device physical addresses; it selects UMA.
- stock physmem_osmem_linux.c PMRSysPhysAddrOSMem (line2604) returns
  page_to_phys for these pages, not a packet-DMA SG address.
- mt8167.dtsi mfg@13000000 (line1012) has the actual GPU resources and no
  external iommus attachment. A distinct g3d_iommu@13005000 node exists;
  its name is not evidence of a second system DMA domain or its runtime state.
- These checked Linux sources are the shipped MT8167 m1.8ED4333936 sources,
  not relabeled as the Windows m1.11 runtime. They establish platform address
  semantics, not Windows operational success or the absence of every IOMMU.

OS protection is preserved by using only the physical MDL from the current
VidMm MAP operation and its native frontend reference. The implementation does
not configure/disable an IOMMU, install identity domains, modify ACPI/UEFI,
change OS security/capability policy, or reuse a freed DMA logical address.
The WDK8.1 interface/operation and actual old-device lifetime stay fixed.
DxgkCbMapMemory maps CPU virtual addresses for assigned translated device
resources; it is not a scatter/gather or DMA-address translation API.

Implementation:
1. Retain only the exact validated WDDM physical backing. No GetSG/PutSG,
   AllocateAdapterChannel/MapTransfer or HAL map-register request remains in
   the native MDL path. The adapter pointer remains an owner identity and
   is still legitimately used for unrelated FW common buffers.
2. GPU PTEs use the retained WDDM PFNs. Existing32bit bounds, duplicate/overlap,
   selected-page, aperture bounds, original-MDL byte checks and native leases
   remain mandatory. No synthetic partial MDL/PFN is constructed here.
3. CPU ownership has an explicit state. Clean the exact locked MDL before GPU
   publication; actual root exposure closes CPU data access. Metadata-only
   identity accessors cannot be used to perform buffer I/O.
4. After real TA/3D/cleanup/SLC/GP completion, invalidate exact target, vertex
   and texture/linear MDL caches before CPU access and scheduler visibility.
   Do not use FactoryCheckpoint or a missing log as a completion receipt.
5. Keep the same residency PTEs and physical lease through CPU ownership until
   WDDM UNMAP plus native job/IRQ retirement. Normal CPU lock/unlock does not
   release HAL DMA resources because this route never owned any. Existing
   stop/reset/power-off proof continues to control destruction/lease release.

No temporary PTE revocation/MMUCACHE transaction from the packet-DMA experiment
is included. No UMD335/334 change, claimed-IRQ patch, extra feature or media is
included. The prior V3 experiment is preserved on fix/gpu332-dma-ownership and
its signed168b integration artifact is retained; it is not this source/binary.

Primary Microsoft contracts:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_buildpagingbuffer
- https://learn.microsoft.com/en-us/windows-hardware/drivers/display/linear-aperture-space-segments
- https://learn.microsoft.com/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_buildpagingbuffer
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkcb_map_memory
- https://learn.microsoft.com/en-us/windows-hardware/drivers/display/iommu-dma-remapping
- https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/using-mdls

Build/sign is integration evidence only. Real reset resolution, cache visibility
and silicon operation remain unverified until main's future combined candidate.
Current device baseline is fresh138, UMD335/KMD332/RTC4b67; no old136 rearm.
