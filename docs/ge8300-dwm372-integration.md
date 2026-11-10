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
transport356.c, display_adapter356.c and miniport.c (the latter also includes
unified_display.h). Confirm dependencies/arguments/tools
against actual receipts before reuse, and recompile any additional real dependent
TU. Re-link both SYS with MSVC14.29 and sign render/transport INF 0.0.3.72.
Unchanged native, UMD, compiler375 and SDK are not rebuilt. The retained UMD is
source30cb, never falsely relabeled as newly compiled from this branch.

Source publication and independent peer review precede the Windows incremental
build. No media/profile or device application belongs to this scope.

## Fixed execution entry (after peer review)

The DWM370 original directory is
`C:/mt8167-dwm30cb20f7-source/build/dwm-source`; its signed baseline is
`C:/mt8167-dwm30cb20f7-signing/build/dwm370-signed/package370-dwm-source`.
Both are read-only inputs. Source/archive/object/tool/signing pins are taken
from the recovered actual DWM370 manifests, not from prospective fresh data.
Four WDK library pins come from the actual native manifest and displib from
the actual SYS link manifest; this does not claim unrecorded per-TU API traces.

After a clean LF clone of this branch, set ProgramFiles(x86) for that child
only to `C:/mt8167-dwm30cb20f7-host/process-sdk`, the existing read-only kit
lookup. No machine environment, SDK setup or shared cache write is performed.

```powershell
python -B tools/ci/build-ge8300-dwm372-incremental.py --enable-candidate --baseline-directory C:/mt8167-dwm30cb20f7-source/build/dwm-source --baseline-package C:/mt8167-dwm30cb20f7-signing/build/dwm370-signed/package370-dwm-source --output build/dwm372-incremental --jobs 4
python -B tools/ci/sign-ge8300-dwm372.py --enable-candidate --baseline-package C:/mt8167-dwm30cb20f7-signing/build/dwm370-signed/package370-dwm-source --linked-directory build/dwm372-incremental --output build/dwm372-signed --sign-tool C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe --inf2cat C:/mt6630-start2015-cache-ae2e0708/closure/small/signing/bin/Inf2Cat.exe
```

`--plan-only` (with a separate fresh output) performs present dependency probes,
archive/tool/input validation and zero compile/link operations. New probes for
historically unrecorded TUs remain compatibility-only. The 38 original native
traces are used as original traces. Any further changed dependency fails closed
with its TU identity instead of silently reusing it or recompiling the world.
Unique header/tool/input hashes are memoized once per verification batch; a
second fresh batch after link detects changed retained inputs. Raw compiler
output goes directly through Python capture rather than PowerShell native error
formatting. The build executes 13 compiles at most4 jobs and2 MSVC14.29 SYS links.
The signer preserves the actual signed source30cb UMD, import library, FW and
clients. Five individual BCBF checks include the retained UMD, plus six catalogue
membership checks. New traces belong only to new compiles. The cold entry is
unchanged. Failure manifests, stdout/stderr and archives are always retained.

Independent review of source802034da approved the production composition and
13TU scope, and required binding the actual link helper environment to the five
pinned WDK library paths. The recipe now resolves those paths from the exact
child environment (case-insensitive Windows variable name), requires canonical
path equality and SHA equality, pins the unchanged helper source, and records
all five bindings before link and verifies them again after link. This guard
requires follow-up peer approval before any Windows compile/sign execution.
