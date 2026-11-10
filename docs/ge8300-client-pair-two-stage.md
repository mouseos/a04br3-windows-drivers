# Client356 + launcher363: two separate one-TU builds

No GPU, SDK, PCO, service, profile, driver-package or device operation is performed.
The runner is existing42848059500. Use disjoint task directories; existing tools,
headers, ARM import libraries and BCBF private-key access are read-only inputs.

1. Freeze the reviewed runtime source (currently main5653962f). Invoke
   `tools/ci/build-ge8300-client-pair.py --phase evaluator --enable-build` with
   its clean checkout and full `--source-commit`. Only dxgi-present356.c is
   compiled, then LINK14.29 and BCBF signing run. Send the actual signed EXE SHA
   and receipt to main. No launcher is built in this stage.
2. Main commits that exact **signed** evaluator SHA in the real launcher363
   `static const BYTE expected[32]` declaration and publishes the new source.
   The recipe does not generate, patch or weaken the launcher source.
3. Only after that formal source commit, invoke `--phase launcher` with the new
   clean checkout, actual `--evaluator`, `--evaluator-receipt` and the exact
   `--evaluator-receipt-sha256`. It rejects any digest mismatch, or changed
   evaluator source/owned includes. Only launcher363.c is compiled, linked and
   signed. Its output contains the two standalone signed EXEs plus receipt.

371 and372 packages remain immutable; no INF/CAT generation or GPU version
change is needed for this client-only sidecar. No profile or stage is generated.
The existing render INF does not install these clients. Main must bind the new
pair's actual hashes in its existing service/media admission when later needed;
this recipe does not silently update old SHA-based contracts or manifests.

## Actual reusable inputs

Recovered memory.obj is `port/xnu/out/windows-rt-console-launch363/job-breakaway/build/memory.obj`,
SHA `0a12d6b58d5fc714e6883772599f29c8b02a8eb8a78d6e90eafbd4f3982e2255`.
The same bytes exist in `port/xnu/out/windows-rt-present356-r2/build/memory.obj`.
Upload a copy to the task's independent input directory if absent on this runner.
Do not assert that the old remote363 directory exists on the new runner.
This is an explicitly SHA-fixed freestanding prebuilt helper; its historical
include closure is unrecorded. Current memory.c must retain source SHA
`3eb20e9c612fe2e5515f2c8e55d57b47dbb0113d8f9d579684b41eb629b6fe94`.
No fabricated historical include/exit information is added.

Existing runner paths, taken from saved actual build/sign receipts:

- clang: `C:/Program Files/LLVM/bin/clang.exe` (pinned57e32b1e).
- LINK14.29: `C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe` (pinnedf8b2e9c5).
- UM/shared headers root: `C:/mt8167-minimal-service-host/cache/kit81/Include`.
- KM headers: `C:/mt8167-public-fullsource323/wdk81/Include/km`.
- ARM libraries: `C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/um/arm`.
- SignTool: `C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe` (pinned854c356a).
- BCBF certificate: `BCBF6C456C6F0DD0818434763B189488FBF13570`.

Evaluator links kernel32/user32/d3d11/dxgi/setupapi; launcher links
kernel32/advapi32/bcrypt. The recipe hashes the actual selected libraries live
before and after link, without claiming those hashes were historical pins.
It records the original new compile-time -MD/-MF trace, one object, actual
commands/exit codes, PE ARMNT/subsystem6.3, signature trust/identity and unchanged
non-signature payload. Failure receipts/logs are retained. No ColdBuild wrapper
is called: build-ge8300-present356.py would also build the GPU and is unsuitable.

After recovery, run the existing source-only RT8.1 import resolver separately:
`tools/ci/verify-ge8300-ui-client-imports.py --project-root ROOT --candidate EXE --out NEW_REPORT`.
Its stored export/API-set directories are local inputs, not a new device read.
Both exported imports and hardware/runtime behavior remain unproven until the
corresponding actual check runs.
