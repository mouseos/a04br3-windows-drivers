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
