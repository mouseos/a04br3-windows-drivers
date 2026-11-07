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

## m1.11 32x32 staging preparation

`present_tile.h` bounds-checks a complete 32x32 32-bit tile against a
CPU-mapped surface's width, height, stride and byte span, and packs its rows
into a distinct 4096-byte CPU staging area. The row helper calculates a
checked offset into a separate destination mapping; it does not perform MMIO
writes, map POST memory for GPU DMA, submit commands or establish a fence.
The actual WDDM path does not call this helper yet. Edge tiles, rotation,
conversion and move overlap require the existing CPU path.

`DxgkCbAcquirePostDisplayOwnership` yields display information and ownership;
its physical address is not a GE8300 DMA logical address. The WDDM present
callback is at PASSIVE_LEVEL and synchronous use must finish before its
source mapping is unlocked. See Microsoft documentation for
[PresentDisplayOnly](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_presentdisplayonly),
[POST display ownership](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkcb_acquire_post_display_ownership),
and [common buffer logical addresses](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pallocate_common_buffer).

The m1.11 diagnostic currently has one frozen fixed-pattern task, one
submission, context cleanup and forced power-off; it is not a repeatable
caller-supplied-pixel copy service. The current Windows fence/cleanup
observations and XNU `NativePowerVRQueue.cpp` UFO wait do not establish a
safe DMA mapping of scanout, cache visibility, repeatability, or stop/removal
lifetime. Until those are implemented and verified, the existing CPU WDDM
path remains active and this helper is not evidence of GPU drawing.

A separate WinPE-only `A04_GE8300_VENDOR_COPY_SERVICE=1` opt-in exposes a
4 KiB METHOD_BUFFERED m1.11 diagnostic transfer. It stages inputs and outputs
through the owner's independent uncached DMA buffers, checks the full output
and guards after task UFO, SLC and GP fence, and never DMA maps POST memory.
The fixed-size client CCB is deliberately not wrapped: once there is no room,
new transfers fail rather than reuse an unverified ring location. A timeout
poisons the queue and retains the owner and DMA buffers; subsequent requests
are refused. D0 exit closes admission and serializes against active requests;
surprise removal closes admission but cannot prove safety of an already active
MMIO transaction, resource retention or fabric DMA stop. This is not connected
to WDDM present and has not been tested on hardware. It is not a production
PnP or suspend solution. The signed diagnostic EXE must not be deployed to
installed Windows.

`A04_GE8300_WDDM_VENDOR_TILE=1` は実験専用で、非回転32bppのちょうど32×32矩形をCPU stageへ詰め、m1.11の同期4KiB IOCTLを発行する。成功時のみ返却stageからCPUがPOST framebufferへ行単位で書く。GPUの直接画面描画ではない。その他の矩形、open/IOCTL失敗、13回上限到達後は既存CPU経路へ戻る。同期完了までは呼出し元stageを保持し、timeout時はproviderが独立したGPU DMA資源を隔離する設計。実機でのcopy、cache可視性、stop・surprise removalは未検証であり、インストール用ではない。
The 81df772 POST diagnostic rebooted after display STEP 06. Its translated
resource count was five, including an OVL IRQ, while the working d76005b
boot had four memory resources and no display IRQ. The BDD ISR returns FALSE.
This is an unproven failure hypothesis, not an observed interrupt storm.
The CPU POST experiment now rejects an IRQ-bearing resource list and its
next candidate must use the previously tested no-display-IRQ firmware.
UART stages 83, 84, 85, 86, 87, and 8A distinguish worker entry, adapter
power callback, GPU device open start/result, common-page IOCTL start, and
unexpected display IRQ rejection. No stage establishes GPU display success.
