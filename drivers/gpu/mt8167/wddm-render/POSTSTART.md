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
