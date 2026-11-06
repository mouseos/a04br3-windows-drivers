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
is independently submitted, SLC-flushed, fenced and CPU-checked. This is a
building block, not a WDDM present interface or an atomic complete frame.
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
