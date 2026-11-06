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
