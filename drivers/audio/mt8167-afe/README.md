# MT8167 AFE / MT6392 audio preparation

This directory contains an **offline** DL1 parameter and position helper, not a
Windows audio driver. It has no registration, hardware access, DMA allocation,
interrupt handler, power management, codec sequence or playback path. No INF or
ACPI device is supplied, so it cannot be installed or enabled. ARM COFF object
compilation does not constitute a WDK driver build or device validation.

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
