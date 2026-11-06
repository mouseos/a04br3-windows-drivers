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

## Kernel-only shared-MDL handoff and inactive tables

With both `A04_GE8300_SHARED_LEASE=1` in the vendor copy provider and
`A04_GE8300_WDDM_DIRECT_EXPERIMENT=1` plus
`A04_GE8300_WDDM_SHARED_LEASE=1` in WDDM, WDDM retries a kernel-only
lease after a synchronous present. The provider grants it only after a
successful existing vendor copy, including its TQ UFO, SLC flush, GP fence,
output verification and readback. It holds a WDF device reference and blocks
normal D0 exit/release while the single token is active. This records local
completion, not a WDDM fence, display visibility or DMA stop.

WDDM allocates and maps the shared MDL using the leased GPU PDO/adapter and
its own display PDO/adapter, then passes the in-kernel owner pointer to the
provider while holding its present mutex. Under the provider lifetime lock,
the provider allocates four independent GPU-adapter common buffers, checks
for aliases against GPU firmware/render/copy buffers and the shared data
pages, builds GPU page-table images and writes them to those buffers. A
collision quarantines the allocation and keeps the lease and MDL. Teardown
unstages these **inactive** tables first, releases both MDL DMA mappings, and
only then releases the GPU lease. An uncertain cleanup retains ownership.
The existing copy service remains usable independently of these new flags.

The GPU DMA table images are not linked into the live render PC root and no
MMU-cache invalidation is submitted. In particular, returning an inactive PC
DMA address does not authorize GPU access. The m1.11 MMUCACHE command, exact
completion/ordering, concurrent context use and timeout retention still need
hardware-specific validation before publishing a parent entry. The interface
also does not establish no-bounce visibility for OVL, frame boundary/latch,
PnP surprise-removal safety or DMA stop. It does not enable direct display.

## Opt-in live root publication and MMUCACHE

`A04_GE8300_SHARED_MMU_PUBLISH=1` is a separate provider build opt-in and
requires the shared lease and vendor copy service. After the existing copy and
its completion fence, with provider lifetime serialization and `command_busy`
held, it requires the client/firmware queues to be idle, verifies the render
FW memory context points at the existing PC, and requires PC slot 512 to be
empty. Slot 512 covers GPU VA `0x8000000000` through `0x803fffffff` (1 GiB),
which is distinct from the existing copy scratch at `0x65c0000000`. It writes
only that PC entry, pointing to the staged 1000-page PD/PT image, then sends an
m1.11 typed `RGXFWIF_KCCB_CMD_MMUCACHE` for the render memory context and waits
for both KCCB consumption and an exact nonzero 16-bit sync value. The existing
copy command stream and its mappings are not modified.

The `0x1f` PC/PD/PT/TLB flags are supported by the exact MTK m1.11ED5425693
DDK: `rgx_fwif_km.h` defines PC=4, PD=2, PT=1 and non-VIVT TLB=0x18;
`rgxmem.c` accumulates these for changed levels and requests TLB for non-VIVT.
The DDK `gaFeatures` entry for 22.0.54.30 is 0x22c24ec05 and the SLC_VIVT
bit is bit 32, which is clear; the captured hardware BVNC is 22.40.54.30.
Source: mouseos/android_kernel_benesse_TAB-A04-BR3 commit
`eb47f42e1f84fe5716ef239620725f82718a620a`, paths
`drivers/misc/mediatek/gpu/gpu_rgx/m1.11ED5425693/services/include/rgx_fwif_km.h`,
`services/server/devices/rgx/rgxmem.c`, and
`hwdefs/km/rgx_bvnc_table_km.h` / `rgx_bvnc_defs_km.h`.
The source submits a 16-bit update to a dedicated sync primitive; our reuse of
the command sync, exact-ack rule and the memory-context field are design
choices not established by that source. Firmware execution and ack semantics
on this Windows/device configuration remain untested. This is an
experiment, not a claim of correct invalidation. Once the root write is
attempted, the shared MDL, both DMA mappings, tables and lease are retained,
including after an acknowledgement. A failed or timed-out MMU command poisons
the existing GPU queue and closes the copy service; retry and release are
forbidden. Normal D0 exit is blocked while leased, but surprise removal and a
full stop/drain are not solved. No render request targets the shared VA and
no OVL switch is performed; display-side bounce/cache visibility and frame
synchronization remain separate, unverified prerequisites.

## Shared GPU copy and OVL combined diagnostic

With the separate provider `A04_GE8300_SHARED_GPU_DIAGNOSTIC=1` and WDDM
`A04_GE8300_WDDM_SHARED_DIAGNOSTIC=1` flags, the existing native22 copy task
is submitted to the already-published shared GPU VA for a fixed top-left
32x32 tile on the 800-pixel, 3200-byte pitch. Under the existing command and
lifetime locks, its PBE CSR and EOT PDS destination are changed for that
one task; after TQ completion, SLC flush and GP fence the old EOT PDS is
restored. The shared MDL is synchronized for a CPU diagnostic read and all
1024 expected words, the private copy destination, source and guards are
checked. A mismatch or uncertain completion poisons the existing copy queue
and retains resources. Success only establishes the diagnostic's CPU view,
not display DMA visibility or a complete initialized frame.

WDDM takes a non-atomic, read-only OVL register snapshot and passes it with
the diagnostic request. Provider UART reports `A04GPU SHARED COPY STATUS`,
`MATCH`, `GUARD`, `PHASE`, `TASK`, `FLUSH`, plus `A04GPU OVL SNAP` fields.
A host can prepare paired test packages with
`tools/ci/build-ge8300-shared-diagnostic.ps1` and explicit original firmware
and native22 USC asset paths. The diagnostic needs the ordinary copy service
to have completed a copy before the shared lease is admitted. The UART output
and success codes are observations to validate on a single boot, not proof of
OVL latch or safe DMA teardown. This script does not install or flash.

`windows_shared_ovl.c` connects the actual shared display DMA address to OVL
switch and restoration operations through WDDM's resource owner. It refuses to
write unless trusted platform callbacks establish both complete-frame contents
and display-side visibility for that exact mapping, as well as an exclusive
safe frame boundary. It also checks the current single-layer, size, pitch,
format and old address. No such callbacks or call site exist: the combined
diagnostic reads OVL only. The 32x32 GPU result is not a complete frame.
Both surfaces and the shared lease remain retained even after a switch or
restore write; no interrupt or readback is treated as retirement.

### Experimental shared tiles and queue wrapping

`A04_GE8300_SHARED_FULL_FRAME=1` (default 0; also requires the shared GPU
diagnostic) enables a kernel-only tile function and a drained-CCB wrapping
experiment. It accepts a stable, packed 32x32 input tile and one of 1000
row-major positions on the fixed 800x1280, 3200-byte-pitch surface. Each tile
is independently submitted, SLC-flushed and fenced. The full-frame path does
not CPU-read the GPU-mapped shared MDL. Queue completion is not pixel
verification, display visibility, or an atomic complete frame.
Normal copy service submissions retain their existing bounded, non-wrapping
behavior. The host simulation exercises 1000 positions and padding; neither
wrapping nor the full surface has been demonstrated on the device.

The m1.11 DDK `services/server/devices/rgx/rgxccb.c` writes a PADDING
header at the old tail and submits it together with the command at zero.
The experiment additionally requires read, write and dependency offsets to
agree before reusing the queue. Whether the running Windows firmware advances
all three in this way is unverified; refusal is safe and expected if it does
not. Timeout poisons the existing owner and retains its published mappings.

A GPU fence and CPU readback do not establish display DMA visibility: the
separately acquired display scatter/gather mapping could use a bounce buffer
whose contents predate GPU writes. No display ownership handoff or remapping
is attempted until GPU drain and mapping lifetime are proven. Linux mt8167
`ddp_path.c` uses CMDQ to wait for RDMA0 EOF or MUTEX0 STREAM EOF before
configuration in video mode; `ddp_manager.c` associates FRAME_DONE with
RDMA0_DONE and IF_VSYNC with DSI0_EXT_TE. A CPU interrupt observed after an
EOF alone does not provide the same serialized operation. CMDQ resources,
event ownership, and safe GPU stop are not established here, so the existing
OVL readiness and frame-boundary gates remain closed.

### Starting the shared diagnostic without a present

`A04_GE8300_WDDM_SHARED_AUTOSTART=1` is a separate, default-off WDDM
option and requires `A04_GE8300_WDDM_SHARED_DIAGNOSTIC=1`. Once display
resources start it creates a bounded worker that retries the existing shared
lease/diagnostic path once per second for at most 120 seconds, independently
of PresentDisplayOnly. It stops on the first successful or retained attempt;
a published mapping is never retried or freed on an uncertain failure. The
worker is stopped and joined before device cleanup and serializes with presents.
UART trace `A04DISP STEP=00000030` records worker creation (or resource
unavailability); `STEP=00000031` records the first attempt, every tenth retry,
and the final result. GPU-side shared diagnostics retain their existing UART
records. Neither starting the worker nor a successful kernel build proves that
WinPE loaded the WDDM driver, owns the display resources, or completed the GPU
diagnostic. This option does not initiate an OVL switch.

### Full-frame present submission and one-way DMA handoff

`A04_GE8300_WDDM_SHARED_PRESENT=1` and provider
`A04_GE8300_SHARED_FULL_FRAME=1` are separate default-off opt-ins. With an
existing published lease, an identity 800x1280 32-bit WDDM present with no
moves and exactly one full-screen dirty rectangle packs the locked source
into 1000 kernel-only METHOD_BUFFERED tile requests. The provider serializes
them under its lifetime lock, requires matching token/frame and strict tile
order, and reports each TQ/SLC/GP fence; failed frames cannot complete. POST
remains the displayed surface. Full submission is synchronous and potentially
very slow; there is no display fence, scanout switch or hardware timing claim.
Partial or rotated presents remain on the original path.

The display handoff helper is deliberately uncalled: it requires independent
proof that the GPU mapping is no longer in the root, MMU caches were
invalidated, GPU DMA drained and OVL cannot access either mapping. It then
releases the old display SG and GPU SG, and obtains a fresh display-read SG.
Its mapping is quarantined on failure. The current OVL gate also requires the
old GPU SG, so it deliberately does not authorize switching after this
handoff; a validated lifecycle and boundary implementation must replace that
condition before the pieces can be connected. Microsoft documents
[GetScatterGatherList](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pget_scatter_gather_list)
and [PutScatterGatherList](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pput_scatter_gather_list)
as transfer-scoped operations, with Put flushing adapter buffers. Neither the
CPU readback in the older 32x32 diagnostic nor a GPU fence proves the new
display mapping is coherent.

### Display resource acquisition UART diagnostics

For diagnostic autostart builds, `A04DISP STEP=0000003F` is the exact
`A04DisplayResources::Start` status (rather than the generic worker status).
Steps 40–4A print integers in the existing STATUS field: 40 is input flags
(PDO present bit 0, device-info present bit 1, matching PDO bit 2, existing
adapter bit 3, existing OVL mapping bit 4, IRQL in bits 8+); 41 is the
translated full-resource-descriptor count (`FFFFFFFF` means null); 42 is
`full-index << 16 | partial-count`; and 43 is `full-index << 24 |
partial-index << 16 | share-disposition << 8 | resource-type`. For each
memory entry among the first 16 partial descriptors, 44/45/46 provide start
low/high and length. 47 encodes OVL match count, IRQ count, and exclusive
OVL/IRQ bits 16/17. Step 48 bits 0–5 indicate adapter, operations, operations
size, GetScatterGatherList, PutScatterGatherList, and PutDmaAdapter; step 49
is the returned map-register count, and 4A reports MMIO mapping status.
Resource data is observational and does not authorize an IRQ or OVL write.

The display ACPI device in the 6d0bfbd firmware had no local `_CCA`; its GPU
sibling declares `_CCA=0`. ARM ACPI requires a coherency declaration for a
DMA-capable device, subject to ancestor inheritance. Device-specific UEFI
change `c19794d5` adds `_CCA=0` conservatively; its effect on Windows RT is
not established until a newly built firmware is tested. The display DMA
adapter request now declares `ScatterGather=FALSE`: OVL uses one linear
address, not a scatter/gather descriptor list. This still requires the OS to
return an appropriate mapping and does not establish bounce, cache, stop, or
frame-boundary safety. An actual failure point cannot be identified from the
old generic `STEP=30 STATUS=C00000A3` alone.

### Common-buffer one-page lease experiment

The observed display adapter supplied 64 map registers. That number bounds
an individual DMA transfer; it does not establish how many mappings can be
retained concurrently, and splitting 1000 pages does not produce the single
contiguous display DMA range required by OVL. In particular, repeatedly
queueing legacy asynchronous SG requests while retaining earlier mappings can
wait indefinitely. Neither adjacent CPU physical pages nor matching logical
addresses prove shared IOMMU domains or the absence of bounce buffers.

The separate default-off `A04_GE8300_SHARED_COMMON_PAGE` provider and
`A04_GE8300_WDDM_SHARED_COMMON_PAGE` display flags enable a 4 KiB diagnostic.
After an existing fenced GPU copy, a kernel-only provider lease pins its
already GPU-mapped uncached common-buffer destination and prevents concurrent
normal copy calls. WDDM builds an MDL over that page and asks the display's
own V3 adapter for a single-page SG mapping with `DMA_SYNCHRONOUS_CALLBACK`;
insufficient resources fail without waiting. The display does not read the
mapping. WDDM releases the adapter object and MDL before the provider lease;
a release failure retains the provider reference and blocks further copies.
Steps 50/51 report lease/mapping status, 52/53 the one-page list and DMA
address, 54/55 the already completed GPU task/flush and 56 lease release.
This validates neither display DMA reads nor visibility, no-bounce, GPU stop,
full-frame mappings, frame boundaries, or OVL switching. The returned GPU
and display DMA addresses belong to separate adapters and cannot be equated.

The full-size shared-MDL path remains behind its existing 1000-register
checks. A valid full scanout design must independently establish a contiguous
4,096,000-byte display logical range, an independently valid GPU mapping of
those same pages, bounded allocation/cleanup, and cache/ownership transitions.
A large `AllocateCommonBuffer` may itself require map registers and can fail;
it does not automatically map the same pages into another device's domain.
Windows 8.1 does not expose the later `CreateCommonBufferFromMdl` interface.
See Microsoft documentation for [common buffers](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pallocate_common_buffer),
[GetScatterGatherListEx](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pget_scatter_gather_list_ex),
and [FreeAdapterObject](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pfree_adapter_object).

### One-shot policy and full-size display common-buffer probe

The one-page probe is limited to one lease request per display-driver object,
including failure. The automatic worker and `PresentDisplayOnly` share this
latch under the present mutex, so successful cleanup (which closes the GPU
handle) cannot re-arm it. Opening the GPU provider may retry until it exists;
no repeated mapping is allowed after the probe starts. A new driver instance
starts with a fresh latch. This does not change the GPU copy-service policy.

With the separate, default-off `A04_GE8300_WDDM_SHARED_FULL_COMMON=1`, a
successful one-page probe is followed once by a 4,096,000-byte uncached
`AllocateCommonBufferEx` request on the display adapter. Step 57 reports the
allocation/linear range check, 58 the display-domain logical base, 59 the
fixed byte length, and 5A that `FreeCommonBuffer` returned. The buffer is
not submitted to OVL. Without the additional common-frame diagnostic it is
freed immediately after the GPU mapping preflight described below
with the same adapter and parameters. Success establishes only that the HAL
provided a display-domain contiguous common-buffer address at that time. It
does not establish GPU access to the same backing pages, no-bounce, visibility,
frame synchronization, or stable capacity on subsequent allocation. Failure
is an expected diagnostic outcome; it neither authorizes a split scanout nor
triggers an allocation retry.

### Full common-buffer cross-adapter mapping preflight

The full display common-buffer probe now runs while the short-lived provider
lease holds the GPU PDO/adapter valid. If display allocation succeeds it
builds an MDL over the common buffer and issues one synchronous V3 GPU-adapter
SG mapping request for the full 4,096,000 bytes. Step 5B records GPU mapping
and layout validation; 5C reports the GPU SG element count on success. A
resource shortage fails without waiting. GPU SG addresses are checked in the
GPU address domain and are not required to be display-contiguous. The display
address still comes exclusively from its common-buffer allocation. The
preflight alone starts no device access; the opt-in continuation below does.

With `A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC=1`,
`A04_GE8300_SHARED_DISPLAY_COMMON=1`, and GPU full-frame support enabled,
success continues in the same boot: the common-page lease is promoted to a
shared lease, the 1000 GPU page addresses are published through the existing
root and MMUCACHE path, and 1000 synthetic 32x32 GPU copies are submitted.
The CPU then compares all 1,024,000 pixels with a position-dependent pattern.
The buffer is initialized with the inverse pattern before the GPU map.
The independent owner, display PDO reference, common buffer, GPU SG mapping,
MDL, transfer context, provider handle and lease remain retained after lease
promotion, including on stage, command or comparison failure. No root removal
or hardware stop is asserted. PnP removal and power transitions remain untested.

Steps 5B/5C report GPU map status and element count; 64 reports lease
promotion, 65 a post-map preparation error, 5D root staging/publication,
5E the completed tile count (or failing zero-based tile), 5F tile status,
62/63 the last task and flush sequence, and 60/61 the CPU match count and
comparison status. 5A/56 appear only when cleanup is permitted. On a GPU map
failure no root or tile is submitted. A CPU match is an observation of this
mapping, not a display-DMA visibility, no-bounce, frame boundary, or stop
proof. OVL switching remains disabled. The common-frame flags default to zero.

### Full-frame observation and freeze

On d76005b hardware UART reports GPU map 5B=0, one element, root 5D=0,
1000 completed tiles, task 0x3ec, flush 0x3ed, and 1,024,000 CPU matches.
This is an observed GPU-to-CPU result for the display common allocation,
not an observed OVL read. Step 31=C00000A3 can be the worker finding that
Present already ran the diagnostic and retained the handle; it is not the
copy result. The worker now reports the completed one-shot result instead.

The provider can seal only a successful complete frame with its exact final
task and flush under the same lock as tile submission. Failure or partial
frame cannot seal; after sealing, later tiles are rejected. The display owner
retains the resulting token, sequences and CPU comparison count separately
from display readiness. Step 66 reports this seal. This prevents the target
surface from being changed through the shared-tile interface after the CPU
comparison, but does not prove PnP quiescence or display DMA visibility.

Windows documents a common buffer as simultaneously accessible to its
allocating device and the processor, with a contiguous device logical address:
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pallocate_common_buffer
and distinguishes processor cache maintenance from adapter flushing:
https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/flushing-cached-data-during-dma-operations
The uncached display-owned allocation and successful CPU observation support
the common-buffer interpretation, but the GPU SG transfer's possible bounce
and the display fabric's actual read have not been independently observed.

The firmware source configures both OVL layers 0 and 3. Linux MT8167
`ddp_ovl.c` uses 0x20-byte layer strides and `ddp_path.c` queues an EOF wait
and register update together through CMDQ. The current Windows ACPI device
describes OVL, RDMA, DSI and display mutex but neither CMDQ nor an interrupt;
the WDDM driver does not own a CMDQ thread or event. A CPU poll followed by
an MMIO write cannot substitute for that transaction. Steps 70-7C now record
a non-atomic, read-only snapshot of OVL enable, source mask, ROI, both layer
controls, sizes, pitches, addresses and interrupt state after the copy. These
reads do not acknowledge an event or authorize a switch. No candidate should
claim a synchronized OVL transition until the missing resource ownership and
execution path are implemented and validated.

### MT8167 CMDQ resource and packet preparation

The device DTS provides GCE MMIO 0x1020a000/0x1000, non-secure GCE SPI 125,
and OVL SPI 160. SPI 126 is the secure GCE IRQ and must not be claimed as a
normal display IRQ. The board ASL has a separately gated resource description
for GCE and GSIs 157/192; it defaults off and by itself grants neither clock
nor thread ownership. Windows can separately opt in to mapping the assigned
GCE range after checking the PnP resource list; that mapping issues no GCE
read, write, interrupt connect or thread submission. Translated IRQ identity,
interrupt routing, power/clock ownership and conflict with secure or firmware
users remain unresolved.

MT8167 DTS maps `disp_rdma0_frame_done` to 27 and `stream_done_0` to 38;
these are hardware event values, unlike the abstract enum values 77 and 113
in `cmdq_event_common.h`. Linux `cmdq_device.c` populates the runtime event
table from DTS. The tested `mt8167_cmdq_packet.h` constructs a Linux-shaped
token clear, non-clearing EOF wait, full L0 address write, EOC and relative JUMP; L3 is
not touched and a packet for the saved L0 address can be built separately.
An alternative inert packet writes both L0 and L3 addresses in order, and a
separate packet can encode their saved values. Neither changes enable, format,
pitch or alpha, and sequential writes alone do not prove an atomic latch.
It only creates an inert image: the clear followed by wait requires exclusive
event ownership, and the boolean precondition is not proof of that ownership.
Display mode and exclusive GCE execution are not established. There is no executor,
ISR, timeout recovery, frame-latch proof or production caller yet.

Linux `cmdq_core.c` resets the owned hardware thread, configures its end and
current address, IRQ enable and priority before enabling it; it patches the
terminal jump/end address according to its task queue. The emitted image here
is not an executable task until an equivalent supported DMA mapping and end
address contract exist. In particular the display adapter used for OVL must
not be assumed to be a GCE DMA adapter merely because both resources appear in
one ACPI device. A GCE PDO/DMA domain and clock/thread/event ownership remain
necessary before this packet can be submitted.
