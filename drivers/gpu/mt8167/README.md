# MT8167 PowerVR power sequencing foundation

`rogue_power.h` is derived from the BSD-2-Clause-Patent
[UniXNU A04BR3 implementation](https://github.com/mouseos/UniXNU-a04br3/blob/main/package/A04BR3RoguePower.h),
which in turn identifies the board Linux clock and power sources. Its callback
interface is a starting point for a Windows port, not a WDDM driver. It is not
connected to PnP, MMIO ownership, clocks, firmware, GPU memory, interrupts or
Direct3D, and must not be applied to a running device on its own.

Power and clock registers are shared with other devices, including touch.
A Windows resource and synchronization design is required before any access.
The optional MMPLL calculation uses 64-bit division; kernel linking on ARM32
has not been established. Tests use mocks only; GPU acceleration is not yet
implemented or verified on Windows RT.

## Rogue MIPS firmware image loading foundation

`firmware.c` validates the open firmware info, layout and 32-bit little-endian
MIPS ELF program headers, then loads the code and data segments into a caller's
16 MiB firmware heap. It follows the upstream PowerVR firmware layout from
[Linux pvr_fw_info.h](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/gpu/drm/imagination/pvr_fw_info.h)
and the board-specific [UniXNU boot implementation](https://github.com/mouseos/UniXNU-a04br3/blob/main/package/powervr/NativePowerVR.cpp).
The firmware is a separate input and is not stored in this repository. The
board implementation uses Imagination's `rogue_22.67.54.30_v1.fw` (SHA-256
`cfe6f58db2ad604b1729349629510c5a3434b266c99ea886d2959887bff4fed8`):
https://gitlab.freedesktop.org/imagination/linux-firmware/-/raw/powervr/powervr/rogue_22.67.54.30_v1.fw

The actual board core is 22.40.54.30. A mismatch is rejected by default and
can only be allowed explicitly; successful image loading does not establish
that the firmware is compatible. This portable code does not allocate DMA
memory, start the firmware, register a Windows device or implement WDDM.
It can also prepare the MIPS boot configuration and the four-page GPU page
table from separately allocated, page-aligned physical heap and table memory
below 4 GiB. The Windows caller must provide and synchronize those allocations;
the prepared data has not been used to start the GPU on Windows.
The optional KMDF provider `mt8167-ge8300.inf` binds the ACPI resource and
offers a status query. Its normal build does not start firmware. The explicitly
enabled `A04_GE8300_BOOT_TEST=1` WinPE build attempts one firmware boot,
health check and shutdown; `A04_GE8300_ALLOW_MISMATCH=1` separately permits
the known firmware/core mismatch. It is not WDDM and is not intended for the
installed Windows system. GPU shutdown, surprise removal, sleep and retained
DMA allocations after failure are not yet verified on real hardware.
`tools/ci/build-ge8300-provider.ps1` fetches the pinned firmware or accepts
`A04_GE8300_FIRMWARE`; in both cases it verifies the SHA-256 before packaging.
`firmware_runtime.c` also constructs the firmware queues and shared system and
OS initialization data. Its ABI structures come from the unchanged upstream
MIT-licensed FWIF headers listed in `fwif/SOURCE`; the Linux compatibility shim
is derived from UniXNU and adjusts `GENMASK` for 32-bit `unsigned long`.
`firmware_start.c` expresses the board's MIPS reset and remap sequence through
MMIO callbacks; returning success only means the sequence was issued. Correct
64-bit MMIO semantics and barriers on ARM32, cache synchronization, resource
ownership and firmware startup polling remain to be implemented and verified.
This sequence must not be connected to live MMIO before those are resolved.
`windows_hw.c` provides an experimental Windows ARM32 adapter: common DMA
buffers use the device adapter's logical addresses, and split 32-bit MMIO
writes follow the low-then-high order in this device's released Linux source.
Common-buffer cache policy is selected by Windows, so the firmware boot and
queue paths explicitly synchronize device reads and writes with MDLs and
`KeFlushIoBuffers`; that behavior still needs verification on this hardware.
The Windows PnP/WDDM owner, DMA adapter setup, GPU quiescence before free and
real-device behavior are not yet implemented or verified.
`windows_firmware.c` composes these steps and waits for the firmware-started
flag. It is not called by a PnP driver and has not been run on the device.
After issuing hardware writes it retains DMA memory even on timeout: a caller
must first establish that GPU DMA is quiescent before releasing that memory.
The shutdown helper sends firmware idle and power-off requests, then checks
GPU idle and outstanding transactions before reset. It frees allocations only
after a successful reset readback. A timed-out command poisons the queue and
retains DMA memory. The Windows PnP power lifecycle and all hardware behavior
still need implementation and device testing.
For a host check with that exact firmware:

```
cc -std=c11 -Wall -Wextra -Werror -I drivers/gpu/mt8167 -I drivers/gpu/mt8167/compat -I drivers/gpu/mt8167/fwif tests/gpu_firmware_image_test.c drivers/gpu/mt8167/firmware.c drivers/gpu/mt8167/firmware_runtime.c drivers/gpu/mt8167/firmware_start.c -o /tmp/a04-gpu-fw-test
/tmp/a04-gpu-fw-test /path/to/rogue_22.67.54.30_v1.fw
cc -std=c11 -Wall -Wextra -Werror -I tests/gpu_windows_stubs -I drivers/gpu/mt8167 -I drivers/gpu/mt8167/compat -I drivers/gpu/mt8167/fwif tests/gpu_windows_firmware_test.c drivers/gpu/mt8167/firmware.c drivers/gpu/mt8167/firmware_runtime.c drivers/gpu/mt8167/firmware_start.c drivers/gpu/mt8167/windows_hw.c drivers/gpu/mt8167/windows_firmware.c -o /tmp/a04-gpu-win-test
/tmp/a04-gpu-win-test /path/to/rogue_22.67.54.30_v1.fw
```
