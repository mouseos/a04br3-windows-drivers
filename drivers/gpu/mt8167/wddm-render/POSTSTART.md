# Windows RT 8.1 post-start contract

GPU3.8 run79 passed actual native owner creation, aperture setup, session start,
draw interfaces and StartDevice, but the adapter remained Code43 and the standard
D3D client exited1 before creating a new log. This is not rendering success.

WDDM1.3 requires GetNodeMetadata. Report only the actual single 3D scheduling node;
reject other adapter/node ordinals and expose no video/presentation feature flags.
SupportNonVGA means StopDeviceAndReleasePostDisplayOwnership support, not simply
non-VGA hardware. A render adapter without POST ownership and without that display
DDI must not claim it. Keep the existing packet-boundary scheduling policy and
per-engine TDR unadvertised; no fake capability flags are introduced.

The actual QueryAdapterInfo types/results and returned caps, plus node metadata
calls, use bounded schema2 records R40..63. They do not change query acceptance,
MMIO, IRQ, shader, or resource ownership. Native owner source remains4260e420;
UMD and firmware remain the signed baseline bytes. Build only miniport and
query_adapter objects, then use Microsoft LINK and the existing signing path.

Sources:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/display/enumerating-gpu-nodes
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_getnodemetadata
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgk_drivercaps

The missing DDI and wrong capability claim are source-contract defects.
The exact runtime validation branch responsible for Code43 remains under REA
investigation. Build/host checks are not device drawing evidence.

## RT8.1 ARM version field

REA of the actual dxgkrnl.sys (SHA256 2f0fe1db882ba83862a6ece3c2de0cf08c2b20767e6640dc4de5b97255b6d941)
shows FUN_000862d4 querying DRIVERCAPS and rejecting WDDMVersion except
0x1000,0x1200,0x1300 for the new DDI. The reported zero therefore cannot pass.
Use DXGKDDI_WDDMv1_3 and compile-time assertions for the actual WDK8.1 ARM
field offset0x148 and enum0x1300. Current documentation describing the field
as reserved is retained as a conflicting newer contract, not silently applied
to this shipped RT binary. Evidence ev_6607dcaa56c726ae672f608b800609f97d07c1042238ed5e6f1e22e425767f06.

## Capability model1.2 with the existing callback ABI

run81 recorded the real DRIVERCAPS query success (output0x220), version1300,
scheduler0x11, one node and no POST/TDR/MPO/flip flags, then Code43 before any
segment or node-metadata query. Standard drawing remains unverified.

FUN_000862d4 also has an RT platform feature gate for model1300, requiring
MPO with multiple overlay planes on hardware adapters unless a software/basic
adapter exemption applies. The candidate owns no presentation/overlay path;
it must not fabricate those flags or mark itself as a software adapter.
Report model1200, which is separately accepted by the actual version gate,
while retaining the current0x4002 callback ABI, actual node metadata, existing
packet-boundary scheduler, native library, UMD, and firmware. This is a model
contract correction; the exact rejected runtime branch still requires device
validation rather than inferring success from this source analysis.

REA references: FUN_000862d4, classification helpers12a88/1d0bc/12aa4 and
FUN_00087bbc. Microsoft basic/soft flags are distinct from a hardware render
adapter with zero display sources. Do not use a software/WARP fallback.


## RT8.1 render interface completeness (3.11)

REA on the same RT8.1 dxgkrnl shows FUN_0008775c requires Present and
ControlInterrupt even with zero display sources. Both now explicitly reject
unsupported display operations; they never return false presentation success,
consume a DMA buffer, control native DMA interrupts, or touch the separate DOD.

That helper also enforces graphics/compute preemption, per-engine TDR and MMIO
vsync flip for platform-enabled model >=1.2. The existing render-only owner has
no vsync flip. The capability model is therefore the explicitly accepted 1.0,
with CancelCommandAware zero and legacy packet-boundary preemption retained.
This decision follows the exact helper condition, rather than testing arbitrary
version numbers. Callback ABI remains 0x4002. Standard Direct3D rendering and
DOD registration remain unverified until the next device receipt.


### 3.11 superseded before device testing by 3.12

Further REA of FUN_00014de0 and FUN_00087534 rejects a hardware render-only
interface below model1.2. Thus 3.11 is not a valid device candidate and was never
flashed. FUN_0008db28 shows the extra-capability gate is the existing registry
ValidateWDDMCaps option, default zero. The captured GraphicsDrivers key is empty,
so there is no evidence that this diagnostic gate was active. Restore model1.2
and CancelCommandAware=1; retain the two missing interface slots as the actual
contract repair. No ValidateWDDMCaps policy modification is introduced.


## Direct3D11 driver-name registration (3.13)

run87 registered both render and DOD with Code0. Its new standard client log
clear-0001a340-00000930 identifies ACPI MOU0006, opens the real PowerVR LUID and
confirms hardware render-only type1. CreateDevice returns DXGI_ERROR_UNSUPPORTED.
Older client logs must not be substituted: the initial collector consumed its
stdin inside adb shell/stat and recovered only the first old file. The collector
was corrected and the actual new log was read without another device experiment.

Microsoft requires a third UserModeDriverName MULTI_SZ entry for D3D11 even
when D3D9/10/11 share one DLL. The former single entry exposed only the legacy
slot. 3.13 fills all three names and corresponding InstalledDisplayDrivers entries.
SYS/DLL/FW remain byte-identical to signed3.12; only INF/CAT are refreshed.
No unsupported operations or feature caps are added. Actual D3D11 rendering
remains unverified pending the next combined peripheral update.

Primary source: https://learn.microsoft.com/en-us/windows-hardware/drivers/display/enabling-support-for-the-direct3d-version-11-ddi
