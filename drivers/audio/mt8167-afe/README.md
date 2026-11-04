# MT8167 AFE / MT6392 audio preparation

This directory contains a software-only WaveRT miniport experiment, not a functioning audio driver. The default adapter refuses StartDevice. A separate diagnostic build can validate resources and attempt to register the miniport, but its stream rejects RUN and it performs no hardware access. No INF is supplied; the standalone ACPI resource proposal remains unintegrated and has _STA=0. A WDK-linked SYS does not establish playback or device validation.

`dl1.c` checks a restricted stereo S16_LE buffer, period and sample rate and
returns candidate DL1 register values, with an inclusive end. Failure leaves
the output untouched. The position helper rejects zero, misaligned and
out-of-buffer current pointers. The caller must provide a contiguous 32-bit
DMA device address and independently establish mapping, coherence, required
hardware alignment, resource ownership and synchronization; four-byte
alignment here is only a PCM-frame constraint, not a claim about DMA alignment.
Only host tests and `make arm-mt8167-dl1` use this helper. It is not connected
to any default build or device.

## Evidence and unresolved integration

The published [A04BR3 Linux source](https://github.com/s1204IT/CT3_GPL_LGPL_Src/tree/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4) contains `arch/arm64/boot/dts/mediatek/a04br3.dts`, `mt8167.dtsi`, `mt6392.dtsi`, `sound/soc/mediatek/mt8167/mt8167-afe-pcm.c`, `mt8167-afe-regs.h`, and `sound/soc/codecs/mt6392-codec.c`. The A04BR3 defconfig selects this DTS. It describes the MT8167 AFE, MT6392 codec and Class D mode, not a demonstrated Windows playback route. Its external amplifier pin states are empty; no external amp model or enable pin is established.

The AFE DTS ranges are 0x11140000+0x1000 and 0x11141000+0x9000, GIC SPI 120 level low; the first overlaps the audiotop syscon. Offsets include DL1 BASE/CUR/END 0x40/0x44/0x48, DAC_CON0/1 0x10/0x14, IRQ control/status/clear/count1 0x3a0/0x3a4/0x3a8/0x3ac. DL1 uses DAC_CON0 bit 1, rate field DAC_CON1 bits 3:0, IRQ1 and a period counter in frames. MT6392 SPK_CON0/9/11 are PMIC offsets 0x52/0x64/0x68; these are **not** AFE MMIO offsets. Linux includes trim and gain sequences; do not copy assumed speaker settings into a hardware path.

Clock dependencies include top audio, APLL1/APLL2, audio engine selectors and divisors; shared top clock registers and the AFE/audiotop overlap require a common owner. MT6392 codec uses PWRAP and APMIXED; the existing Windows button provider owns PWRAP and currently has no cross-driver PMIC sharing. UniXNU reset/input/GPU code also documents WACS2 ordering and contention, but its ownership is not a Windows arbitration service. The codec, its rails (DTS lists VAUD22/VAUD28), analog route, trim, power ordering, firmware handoff, pin state and ACPI resources must be established before enabling hardware. Linux source values and an existing VirtualAudio isolation log are not evidence of audible Windows or physical speaker playback. No new device interaction occurred.

Future work: agree a shared PMIC/clock ownership interface; determine the exact board route and safe power/volume/trim; implement Windows 8.1 PortCls/WaveRT or another supported audio path, DMA and cache coherency, interrupts, power transitions, ACPI/INF and signing; then validate under an explicitly authorized hardware session.

`ring.c` plans frame-aligned wraparound spans without accessing DMA memory; `irq.c` classifies DL1 status without reading or acknowledging hardware. No caller connects either to the PortCls skeleton. See [ownership.md](ownership.md) for required single-owner arbitration. The A04BR3 Linux defconfig enables CONFIG_MTK_SPEAKER and its machine driver contains an internal MT6392 speaker route from AU_LOL, but these facts do not establish the physical speaker wiring or safe gain.

`dl1_engine.c` implements a callback-based DL1 register sequence and IRQ1
classification/acknowledgment, currently exercised only with mock MMIO. Its
caller must provide exclusive, ordered and serialized ownership, a coherent
mapped DMA buffer, interrupt synchronization, clocks and a muted codec. Errors
poison the engine; neither a successful stop nor enable-bit readback proves
DMA drained. The adapter has no callback transport and cannot start it.

`wavert_dma.cpp` uses the PortCls stream to allocate physically contiguous,
noncached pages, checks the 32-bit range and refuses to release a buffer once
handed to hardware. Physical addresses are not assumed to be DMA device
addresses: Windows DMA mapping/coherency and the conditions for safe drain
and release remain unresolved. The diagnostic WaveRT stream uses this for allocations
which have not been exposed to hardware. The owner of shared PWRAP/clock resources is not implemented;
no caller may enable these paths based on this code alone.

The optional `wavert_registration.cpp` wraps PcNewPort/Init/
PcRegisterSubdevice. A diagnostic-only adapter creates a minimal WaveRT
miniport and attempts registration after validating resource descriptors. The experimental
`irq_windows.cpp` connects an exclusive translated interrupt and dispatches
DL1 events via a DPC, then disconnects and flushes DPCs. It is not connected
to PnP, DMA, clocks or a miniport; DPC coalescing and failure/interrupt-storm
recovery require a real stream design. The caller must use interrupt
synchronization for all concurrent engine access and hold resources alive.
Both helpers are compiled out unless A04_MT8167_WAVERT_EXPERIMENTAL=1.
The adapter additionally requires A04_MT8167_WAVERT_DIAGNOSTIC=1 to attempt
registration; the default build always rejects StartDevice. The diagnostic
miniport supports only stereo 16-bit 48 kHz PCM, refuses KSSTATE_RUN, and
returns STATUS_DEVICE_NOT_READY for GetDeviceDescription. It has no hardware
DMA, interrupts, topology registration, synchronized teardown or power path.
Registration and stream behavior have not been tested in Windows. The ACPI
resource check does not establish exclusive ownership, or validate the
translated interrupt vector and affinity. There is no valid INF.
