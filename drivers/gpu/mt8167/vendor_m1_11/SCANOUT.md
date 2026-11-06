# GE8300 direct scanout: ownership policy

`scanout_candidate.h` checks only geometry and physical continuity. Equality
with the GPU adapter address is not a display DMA mapping. The Linux MT8167
`ddp_ovl.c` programs `OVL_L0_ADDR` from the layer address, including the M4U
path for secure transactions, and programs pitch separately. It disables OVL
in `ovl_stop` but checks `OVL_FLOW_CTRL_DBG` separately in `ovl_is_idle`.
Neither a register write nor the GPU fence by itself proves both devices idle.

`scanout_lifetime.h` is a host-tested policy, not an OVL driver. Its caller
must serialize operations, own mappings for both adapters covering the same
allocation, provide a display-visible contiguous 32-bit range, and retain both
mappings across all uncertain outcomes. The exact fence must complete and the
GPU writes must be visible to the display before requesting a switch. An OVL
transaction needs an independently established latch confirmation; an interrupt
or register read must not be assumed to prove this without hardware evidence.
Stop requires independently established GPU and display quiescence. A failed
switch, visibility operation or stop leaves mappings retained. The owner must
also keep the previously active surface until its retirement is established.

The UniXNU `package/powervr/NativePowerVRGpu.cpp` scanout import retains the
existing mapping and marks it to bypass the GPU device cache; that is evidence
for treating scanout lifetime and cache domain explicitly, not proof that the
same mechanism works on Windows. Missing pieces include a display-side DMA
adapter mapping, cache maintenance/visibility, hardware-specific latch and
retirement evidence, stop/removal integration and a WDDM presentation path.
This policy is deliberately not wired to hardware and does not enable direct
scanout.

## Experimental Windows mapping and OVL write

`windows_display_scanout.c` compiles only when
`A04_GE8300_DISPLAY_SCANOUT_EXPERIMENT=1`; the default provider contains no
active path. It maps a caller-owned locked MDL with the **OVL device's** DMA
adapter using GetScatterGatherList for device reads. Every MDL PFN must match
the backing of the prepared GPU common buffer; the display scatter/gather
addresses must form one contiguous 32-bit interval. The SG mapping and its
adapter, MDL and GPU surface must stay owned for the full display lifetime.
Map before the GPU surface is marked hardware-exposed. The display adapter and
PDO must actually describe OVL and its M4U domain; this code cannot verify that
platform resource assignment. DMA bounce buffers require the readiness
operation to synchronize the display mapping after GPU completion; merely
obtaining the list is not sufficient.

The switch routine joins that mapping to the MT8167 OVL L0 address register.
It requires a PnP-owned mapped OVL range and a display lease held by the caller,
a real exact-fence/visibility operation, and a real safe-boundary operation.
It validates enabled OVL, a single active layer, memory source, a 32-bit
format, no hardware flip, geometry, pitch, and the
expected existing address before writing. The caller must also establish that
L0 format, rotation and pixel layout match the rendered surface. Current
firmware config enables layer 3, so this implementation deliberately refuses
it. No vblank or register read is claimed to prove a latch. Once a write is
attempted the mapping and GPU surface cannot be freed by this interface; it
provides a guarded write of the saved address for restoration, but neither
write proves latch or retirement. Stop/removal and resource release remain
unimplemented. No production WDDM/provider entry point registers or calls it.
The current screen diagnostic uses pitch 3328 and cannot use this 3200-pitch
path. Thus enabling compilation does not enable scanout or demonstrate a
working display. Its host test models DMA and MMIO; it is not a hardware test.

## WDDM resource acquisition

With `A04_GE8300_WDDM_DIRECT_EXPERIMENT=1`, WDDM StartDevice now obtains the
translated PnP resources and, only when a unique exclusive OVL0 memory range
is assigned, requests a 32-bit scatter/gather adapter from the same display
PDO and maps that assigned range uncached. The adapter must provide at least
1000 map registers. StopDevice releases these resources; no scanout mapping or
hardware write is currently connected to this object, so no live DMA can be
released by this cleanup. A missing or unsuitable resource falls back to the
existing CPU display path. `DisplayProvider.asl` currently provides OVL0,
RDMA0, DSI0 and display-mutex memory but no interrupt. An optional exclusive
interrupt is recognized, but no ISR is registered or enabled by this change.
Linux MT8167 DTS specifies OVL0 SPI 160 (ACPI GSIV 192); the translation and
exclusive ownership must be established through PnP, not a hardcoded vector.

The GPU provider and display miniport are separate device stacks. No shared
surface/MDL lease connects them yet. The diagnostic TQ UFO, SLC flush and GP
fence are not a WDDM present fence, and the new WDDM resource object does not
supply the `ready` or `boundary` callbacks. OVL frame-complete bit 1 in Linux
is an observation, not by itself proof of an atomic switch or retirement.
A PnP interrupt, an ISR-safe transaction or hardware command queue, shared
allocation lifetime, and device-to-device visibility are still required before
this experimental code can safely submit a scanout switch.

## Fenced WDDM tile staging

`A04_GE8300_VENDOR_FENCED_COPY=1` adds a kernel-requestor-only buffered IOCTL
when the separate vendor copy service is explicitly enabled. It returns the
4 KiB CPU-staged result and the exact locally observed TQ task and subsequent
SLC-flush/GP-fence sequences, but only after output and guard verification and
readback succeed. Failure returns no result and follows the existing poisoned
queue/retention policy. `A04_GE8300_WDDM_FENCED_TILE=1` additionally requires
`A04_GE8300_WDDM_VENDOR_TILE=1`; it verifies the response before the synchronous
WDDM present writes the tile to POST. Missing or invalid response falls back to
CPU. This ties the existing bounded copy completion to the WDDM synchronous
present, and does not turn the sequence values into Dxgkrnl fences.

The GPU source/destination remain private bounce allocations and the CPU still
writes POST. This does not share a GPU scanout surface or establish GPU-to-OVL
cache visibility, IRQ ownership, atomic frame switching or retirement. These
prerequisites remain required for the separate direct scanout path.

## Shared MDL preparation

`windows_shared_scanout.c` allocates one page-backed MDL through
`MmAllocatePagesForMdlEx`, then obtains independent GPU and OVL DMA mappings
from their respective PDOs and DMA adapters. The GPU mapping is for device
writes (`WriteToDevice=FALSE`); OVL is for device reads (`TRUE`). The same
flags are supplied when each mapping is released. This also corrects the
previous `windows_display_scanout.c` OVL direction. The helper converts the
GPU scatter/gather list to 1000 GPU pages, rejects duplicate GPU addresses,
requires a contiguous 32-bit OVL address, and can construct inactive GPU MMU
page-table images with SLC bypass. GPU owner-supplied page-table DMA addresses
must be distinct from each other and from every mapped GPU data page.

The WDDM resource owner now has a `PrepareShared` entry point that supplies
its PnP-assigned display PDO and adapter; the GPU owner must supply and retain
its PDO and adapter until cleanup. This entry point has no production caller.
No table image is published, no GPU work or OVL write is submitted, and no
mapping is exposed to either device by this integration. The existing GPU
service is separately reported by the user to have completed three 4 KiB
copies with matching CRC in
`port/gsi/logs/ios-feasibility/windows-rt-ge8300-copy-43c59835-uart.log`;
that does not validate this allocation or a scanout.

A DMA adapter can introduce bounce buffers: an OVL mapping made before GPU
writes may see a stale copy. Thus even a successful dual mapping, uncached MDL,
GPU SLC bypass and a fence are not accepted here as proof of display visibility.
A platform-backed no-bounce/visibility contract, shared cross-driver ownership,
GPU table publication/invalidation, present completion, frame switching and
both engines' stop/removal protocol remain necessary. Unexposed mappings can
be released; exposed mappings are retained. The current WDDM lifecycle only
supports unexposed preparation and must be redesigned before hardware use.
