# 4 CPU execution diagnostic

`fourcpu-load.exe` is an optional user-mode ARM32 Windows RT diagnostic. It
checks process affinity for logical CPUs 0–3, pins four threads individually,
checks the executing logical CPU during a bounded 60-second arithmetic load,
and reports per-thread status and iteration counts. A failure, timeout,
missing output or a watchdog reset is not a pass. It performs no device I/O and
does not alter firmware or installed drivers. It has not been run on the tablet.

Build from a checkout with the Windows 8.1 SDK ARM32 libraries and Clang/LLD:
`powershell -File tools/ci/build-fourcpu-load.ps1`. The resulting executable
is unsigned; the target's authorization and signature policy is unverified.
A 60-second pass is evidence only of this workload on four logical CPUs. It
does not establish interrupt correctness, physical CPU identity, long-term
stability, suspend/resume, temperature margins, or watchdog recovery.
