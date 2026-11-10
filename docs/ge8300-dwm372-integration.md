# DWM372: DWM30cb + DTM0 timing + unified NOTSPEC

The production base is DWM30cb, with the opt-in c2ed7013 direct-I/O resume
recipe retained. This branch composes only timing source
42d8dd5fcc9b3f0dabc2dd1821900b61945218ef and NOTSPEC source
b63827d7c45e099d5844e2c694804a1df047ed10. Their nine production files are
byte-identical to those source commits. The .gitattributes conflict was resolved
by retaining both sets of LF rules. Fullsource origin rows retain their complete
predecessors, including changed legacy BDD files; the native 67-TU routing and
compiler375 source plan are unchanged.

Candidate version 0.0.3.72 is reserved. DWM370 and GPU371 products remain frozen.
No unrelated output-enumeration change, frequency fallback, relaxed registry
predicate, new diagnostic client or cap advertisement is added.

Descriptor geometry, real partial RT-to-staging transfer, scissor and bounded
premultiplied ONE/INV_SRC_ALPHA ADD are retained from source30cb. The physical
panel/POST path remains 800x1280 identity, while offscreen resource geometry
remains descriptor-derived; these are distinct contracts. NOTSPEC is resolved
only as implemented by the supplied main commit. Timing requires the existing
DIS0.DTM0 64-byte boot record and v2 provider interface, with no v1 fallback.

Boot98de is retained. This repository makes no UEFI/EDK2, boot image, device,
media, controller or deployment changes. Hardware/DWM verification is false.

## Incremental provenance

Use only the actual DWM370 source30cb Windows build and its object/library/tool
hashes and original raw logs. Native has 67 real objects: 38 have original
compile-time dependency traces; the first 29 have no historical include/exit
trace and must retain that explicit limitation. A new dependency or preprocessing
probe can prove present compatibility, not manufacture historical evidence.
Preserve each retained product's original source30cb provenance. Hash unique
inputs once per batch, not once per TU. Reuse no unrelated old SDK/PCO artifact.

The expected changed driver scope is all ten BDD C++ TUs (bdd.hxx layout), plus
transport356.c and display_adapter356.c. Confirm dependencies/arguments/tools
against actual receipts before reuse, and recompile any additional real dependent
TU. Re-link both SYS with MSVC14.29 and sign render/transport INF 0.0.3.72.
Unchanged native, UMD, compiler375 and SDK are not rebuilt. The retained UMD is
source30cb, never falsely relabeled as newly compiled from this branch.

Source publication and independent peer review precede the Windows incremental
build. No media/profile or device application belongs to this scope.
