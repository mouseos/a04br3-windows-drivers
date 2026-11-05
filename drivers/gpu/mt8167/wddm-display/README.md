# Windows RT 8.1 display-only build foundation

This is a Windows 8.1 ARM32 adaptation of Microsoft's KMDOD sample, with the
upstream source and MS-PL license identified in `SOURCE`. Run
`tools/ci/build-wddm-display-arm.ps1` with Windows SDK/WDK 8.1, an ARM32 MSVC
compiler, Windows UCRT headers, and the matching MSVC linker installed. The default script
builds a link check. A separate `tools/ci/build-wddm-display-package-arm.ps1`
creates an experimental catalog and INF for ACPI `MOU0007`; this package is
not ready to install until PnP, power, teardown and recovery are verified.

This code does not yet acquire TAB-A04-BR3 display and GE8300 resources, start
the GPU itself or implement a full graphics driver or Direct3D UMD.
An opt-in `A04_GE8300_WDDM_COPY=1` build can attempt fixed 16 KiB copies via
the separate experimental GE8300 kernel provider for full-width, unrotated,
contiguous 32-bit rectangles. All other cases, provider absence and any
failure use the existing CPU path. Both sides are disabled by default. The
provider stages through CPU memory, so a speedup is not established.
In the WinPE device test of driver commit `141466c`, the display miniport
started and the provider reported two successful 16 KiB display-copy requests;
the framebuffer was captured. These requests include CPU staging and do not
establish faster presentation, general 3D rendering, or production stability.
That copy path targets the earlier, different firmware ABI. It is not a
validated presentation path for the device's m1.11 firmware: the m1.11 TQ
diagnostic currently writes only to its dedicated 32-by-32 scratch buffer.
Do not connect the older copy producer to m1.11 without establishing its
command ABI, resource ownership, synchronization, and teardown separately.
The later power-exit changes have not been tested on the device.
The upstream sample explicitly does not support sleep. Do not deploy this build
on the device until resource ownership, GPU synchronization, reset and power
transitions have been implemented and independently verified.

The render diagnostic's fixed triangle and clear passed on hardware separately
from this display-only path. They do not validate WDDM presentation, shared
surfaces, GPU DMA quiescence, a graphics KMD or a Direct3D UMD. The opt-in
copy provider now retains DMA allocations and refuses D0 exit when its transfer
context exists; firmware shutdown alone is not proof of DMA quiescence. This
conservative behavior blocks normal suspend/removal and must be replaced by a
verified quiesce and recovery sequence before device deployment.

The display-only present path finishes synchronously while holding its present
mutex. StopDevice and StopDeviceAndReleasePostDisplayOwnership now take that
same mutex before closing admission and unmapping the POST framebuffer; the
latter also serializes blackout and display-info handoff. A new present after
stop is rejected. This protects these paths only, and does not establish mode
change, power transition, surprise removal, crash-display or GPU DMA lifetime.
POST physical addresses are not DMA addresses and must not be passed directly
to GE8300 without an OS-authorized mapping and retained ownership.
