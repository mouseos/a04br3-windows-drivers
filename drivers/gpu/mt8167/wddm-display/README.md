# Windows RT 8.1 display-only build foundation

This is a Windows 8.1 ARM32 adaptation of Microsoft's KMDOD sample, with the
upstream source and MS-PL license identified in `SOURCE`. Run
`tools/ci/build-wddm-display-arm.ps1` with Windows SDK/WDK 8.1, an ARM32 MSVC
compiler, Windows UCRT headers, and LLVM linker installed. It builds a link
check with no INF and is not installable as supplied.

This code does not yet acquire TAB-A04-BR3 display and GE8300 resources, start
the GPU, use GPU copy, or implement a full graphics driver or Direct3D UMD.
The upstream sample explicitly does not support sleep. Do not deploy this build
on the device until resource ownership, GPU synchronization, reset and power
transitions have been implemented and independently verified.
