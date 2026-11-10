# DWM370: source30cb same-invocation native resume

This opt-in recipe does not change the cold entry or compile source. Use only
with clean `30cb20f79ec3c5f71c056d5783fe6743e92bf2ce`, its existing verified
adapter-derived compiler cache, and its native-only interrupted build directory.
The kernel compiler/linker remain the pinned MSVC14.29 pair. SDK and Mesa365
rebuild counts remain zero. Timing369 is absent; deployment is prohibited.

`checkpoint-ge8300-dwm30cb.ps1 -EnableHandoff` selects one actual in-flight
clang child of the explicitly named build process and waits for its real exit0
before stopping that serial scheduler. A possible newly-started successor is
explicitly incomplete, excluded from reuse, and quarantined rather than erased.
It never cancels the selected in-flight TU. All completed object hashes and the
original logs survive. Historical per-TU exit codes and include traces were not
recorded by the original entry and remain unrecorded; they are not reconstructed
as fabricated successful strict include evidence.

`resume-ge8300-dwm30cb.ps1 -EnableResume -NativeJobs 4` derives the unit/flag
plan from the hash-pinned original native recipe, retains exactly the handoff
objects after SHA verification, and compiles only missing units. Each new compile
writes stdout/stderr directly to files, records actual exit/timing/arguments,
and emits its original compile-time `.d` trace with `-MD -MF`. No later probe is
represented as the historical include trace. The original native archive and
metadata expressions and original KMD/UMD/link continuation are then evaluated
against the clean original source. Final manifests explicitly record this recipe
commit, controlled handoff, retained object count and reconstructed metadata.

`-PlanOnly` emits the exact plan without compiling or requiring a handoff. For
later original module scripts, a process-local `clang` function invokes the real
compiler with identical arguments and saves raw diagnostics through the same
Python direct-I/O helper. It does not suppress diagnostic generation, alter code
flags, edit PATH, modify shared SDK/cache data or install any tool. The real clang
path/hash is pinned by the handoff. This function is removed on completion.
