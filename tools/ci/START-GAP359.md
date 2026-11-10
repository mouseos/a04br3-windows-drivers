# GPU359 startup gap observation

358 baseline stays frozen. Three changed TUs: miniport, display_adapter356, BDD.
The added header is included only in these TUs. Existing return values, ownership,
capacity, pixel format21, physical/DMA predicates and callback order are preserved.
No hardware call, MMIO access, forced flush or power IRP hold is added.

Four fixed384B records have entry/return time and step entered/completed masks.
Uncompleted/sentinel/stale/absent zero is not a successful return. Observe actual
AcquirePost callback status and POST layout before BDD's existing status folding.
A POST physical address remains a semantic observation, never a GPU DMA address.
No pointers or contents of framebuffer/user surfaces are stored.

Source-only equivalence: python tools/ci/test-ge8300-start-gap359.py.
Incremental/sign recipe: tools/ci/build-ge8300-start-gap359.py using exact358 cache.
raw80 changes1, other79 unchanged; two additional display objects rebuilt.
UMD22/PCO36/native67/DIS0/FW/client remain byte-identical. Final six signature and
six catalog membership logs are included under package359/proof359 with normalized
forward-slash paths and hashes in signed-manifest. Original negative proofs stay.

Source evidence: matchedFW37129f1b uses SimpleFb vendorGUID GOP path, not a GPU0
ACPI path. This alone proves neither POST attribution nor callback failure.
Microsoft POST ownership contract allows Width0 for non-POST/no-current-info;
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkcb_acquire_post_display_ownership
No FW/common-EDK change is justified until actual return/layout establishes it.
Record contract/reader are tools/recovery/ge8300-start-gap359-contract.json and
read-ge8300-start-gap359.ps1. Hardware/main-only evaluation remains required.
