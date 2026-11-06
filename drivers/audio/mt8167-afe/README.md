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

`dma_adapter.cpp` obtains a Windows bus-master DMA adapter for a caller-supplied
PDO. `common_dma.cpp` allocates a noncached common buffer, checks the returned
**logical** address and 32-bit bounds, builds an MDL and offers a pre-publication
`KeFlushIoBuffers` operation. These are currently unconnected helpers. The
caller must hold the adapter, stop concurrent CPU writes before preparation,
confirm the platform's cache contract, and establish secure PnP/clock ownership.
The helper never frees an exposed buffer; DL1 stop-bit readback does not prove
that outstanding DMA has drained. It does not handle synchronization for a
live cyclic buffer. No hardware can be started through the current miniport.
See [Microsoft's common-buffer procedure](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/using-common-buffer-system-dma)
and [WaveRT AllocateAudioBuffer](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertstream-allocateaudiobuffer).

`stream_transport.cpp` is an internal experiment that joins a pre-established
clock lease, common-buffer preparation, IRQ connection and synchronized DL1
start/stop. It has no WaveRT or PnP caller: a real parent must establish
exclusive ownership, clock/codec safety, cache coherency and IRQ resources. The
initial buffer must not be modified concurrently; live cyclic cache management
is not implemented. Once an address may have reached hardware, failed start
and all stop outcomes retain the buffer and clock. A stopped enable bit is not
DMA-drain evidence. On stop failure the IRQ remains connected, so its handler,
engine, mapping and callback must remain valid; failure and interrupt-storm
recovery remain unresolved.

The Windows IRQ wrapper serializes DL1 start/stop with its ISR through
KeSynchronizeExecution and accumulates serviced IRQ notifications across DPC
coalescing. This count is only ISR calls serviced by software, not proof of
elapsed hardware periods. Fault notification is sticky. Both transport and
IRQ paths are experimental, unconnected and untested in Windows on this SoC.
The IRQ helper also provides a nonblocking synchronized DL1 CUR read that
rejects non-running, faulted, out-of-range and unaligned positions without
changing its output. Its caller must keep the mapping and interrupt alive and
exclude disconnect. The transport saves the active DMA plan and exposes this
ring offset; it is not a monotonic elapsed-byte count. The diagnostic PnP
backend forwards WaveRT position queries to its own transport under rundown
protection; PnP revoke waits for these queries before unmapping AFE. Its
transport never reaches RUNNING, so the query still returns not ready. The PnP
backend stores the validated translated IRQ, but no operational PnP path
connects or starts it. A future start/stop implementation must synchronize
transport state with position readers as well as ISR and rundown.

The board Linux IRQ handler reads AFE_IRQ_STATUS and AFE_DAC_CON0, reports a
period only for an active memif, then clears pending AFE IRQ bits. The DL1
experiment acknowledges only IRQ1, preserving other sources. A host integration
test joins a mock parent lease, transport, IRQ wrapper and register engine; it
is not evidence that the actual Windows interrupt or board codec works.

`mt8167_dl1_prepare_masked_irq` implements a pre-connect DL1 mask, latch clear,
and readback for an exclusive owner with a held clock. It refuses an active DL1
and poisons on ambiguous I/O. It deliberately leaves other IRQ sources alone;
the shared owner must exclude those sources and keep the mask stable until
disconnect. It is not used directly by the PnP path.

The separate `A04_AUDIO_IRQ_DIAGNOSTIC=1` path requires both the mapped child
AFE resource and a version 4 shared-parent interface. It acquires a muted
clock/codec lease and calls the parent's `PrepareMaskedIrq` contract, which
must establish exclusive AFE ownership, initially mask every source, and keep
all non-DL1 sources masked until lease release. After ISR connection only the
child may unmask DL1; it must mask and synchronize it before releasing. Only then does the backend connect the PnP-translated IRQ.
PnP revoke drains position requests, disconnects and flushes the IRQ, releases
the lease, then unmaps AFE. Failed lease release retains the owner, mapping and
parent references. The prototype parent rejects both acquisition and preparation
without MMIO, so this path cannot connect on that parent. It does not start DMA,
unmask interrupts or allow RUN. Version 4 intentionally rejects older parents.

The miniport now accepts an optional internal `Mt8167StreamBackend` supplied by
an owning PnP provider. It forwards allocation, format-independent start/stop,
position and device-description requests and retains a backend reference for
stream lifetime. The current adapter never supplies one. In its absence the
diagnostic stream still refuses RUN. Once a start may have exposed DMA, the
stream intentionally retains the backend reference and buffer even if stop
succeeds: the platform has no proven DMA drain procedure. The backend can
explicitly report a failure before any possible exposure, allowing release.
An ambiguous failure remains quarantined. A failed
stop is not retried. This is a quarantine/leak, not a finished PnP removal or
power-management solution. A future provider must enforce serialization,
resource ownership and stop/failure recovery; the interface itself does not
prove any of them.

With all three diagnostic flags (`A04_MT8167_WAVERT_EXPERIMENTAL`,
`A04_MT8167_WAVERT_DIAGNOSTIC`, `A04_MT8167_OWNER_DIAGNOSTIC`) set to 1,
the adapter gets its PDO via PortCls, creates a referenced backend with a Windows
DMA adapter and passes it to the WaveRT miniport. The backend allocates only
unexposed common buffers and always rejects RUN and device-description requests.
PnP stop, surprise removal and remove revoke the owner and drop the adapter's
reference before forwarding the IRP to PortCls; outstanding stream references
keep the owner and DMA adapter alive. This is untested diagnostic code, not a
verified Windows PnP/remove/power implementation. It has no INF, its ACPI node
is disabled, and it never maps AFE, connects IRQ, starts DMA or programs power.
Default builds compile the owner path out.

The board Linux driver disables DL1 and then disables/clears IRQ on STOP, but
its code does not explain when outstanding bus transfers have completed.
The mainline [MT8365 AFE driver](https://github.com/torvalds/linux/blob/master/sound/soc/mediatek/mt8365/mt8365-afe-pcm.c)
is another SoC and cannot establish MT8167 drain semantics. Neither a cleared
bit nor a stationary CUR is accepted as a drain proof. No backend release of a
hardware-exposed common buffer has been added.

The diagnostic PnP backend copies only one exclusive, level-sensitive
translated IRQ descriptor (vector, level and affinity); it refuses access after
revocation. It does **not** connect that IRQ. The observed descriptor alone is
not proof of the correct route or ownership of shared clock/PWRAP resources.
The MT8167 board driver's STOP path clears the DL1 enable and disables IRQ;
its source does not document drain or outstanding bus transactions. The
[mainline MediaTek common FE STOP implementation at 5542791](https://github.com/torvalds/linux/blob/5542791d509d07d5e2e9d089f6e0b805c755eee8/sound/soc/mediatek/common/mtk-afe-fe-dai.c)
also clears a memif enable bit, but neither its source nor the MT8365 driver
establishes the MT8167 drain contract. Do not use this as evidence for
freeing exposed DMA or unmapping registers.

PnP IRQ parsing rejects message-signaled and edge-triggered descriptors,
zero vector or affinity, levels above HIGH_LEVEL, and (where processor groups
are enabled) a nonzero group that IoConnectInterrupt's group-zero affinity
cannot represent. It checks both raw and translated descriptors for exclusive,
level-triggered line interrupts and uses only the translated vector for a future
connection. A translated Windows vector is not assumed to equal the raw ACPI
GSI. This only validates the descriptor shape; the owner still does not
connect the interrupt. The existing button provider continues to directly own
PWRAP, while the shared clock owner remains a non-PnP policy component.

WaveRT GetPosition may run at DISPATCH_LEVEL according to the
[Microsoft GetPosition contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertstream-getposition).
A future backend position callback must therefore be nonpaged, nonblocking,
and synchronized with ISR and teardown. Atomic stream state alone does not
protect a callback already in flight or establish hardware ownership.

The board Linux driver names AFE_MEMIF_MON0..3 and dumps those registers but
provides no bit definitions proving DMA bus drain. The common offload
`AUDIO_DRAIN_*` names describe an audio-offload operation and are not evidence
for the DL1 memory-interface bus transaction completion. Do not turn either
into a release criterion without a device-specific specification or validation.

`shared_audio.c` defines an internal, versioned client contract for a future
single parent that owns the audio clocks and PMIC/codec mute together. PnP
backend can retain that contract and refuses attachment after revocation. The
existing input-client and offline parent interfaces do not implement this
contract and are not assumed to be the same interface. There is no operational
provider, PWRAP codec write, or shared clock implementation.
`shared_transport.cpp` joins that hypothetical lease to the existing DL1/IRQ
transport but has no PnP caller. It releases the shared lease if validation or
DMA preparation fails before IRQ connection; a failed release quarantines it.
It quarantines the shared reference and lease before attempting IRQ connection,
because an ISR may run immediately; no post-exposure drain proof exists.
Neither this composition nor a successful callback proves the provider owns
hardware. No actual IRQ is connected by the present PnP backend; RUN remains
refused, and this code is not a safe device trial.

`audio_parent_client.c` bridges a successfully queried, referenced
`A04_SHARED_AUDIO_INTERFACE` to the internal audio shared-service contract.
Binding transfers the query reference, attachment holds an additional parent
reference, and close is refused while any clients remain. The caller must
serialize lifecycle calls. With `A04_AUDIO_QUERY_SHARED=1`, the diagnostic PnP
backend sends a query to its child PDO and attaches this object. The parent
prototype publishes and forwards the same versioned GUID to a gated audio
child, but its audio callbacks always fail and never touch codec or clock.
The child's AFE resource candidates are gated and there is no installable driver binding. No actual audio
clock/PWRAP lease, DMA start, interrupt connection or playback is enabled.

The shared transport uses the parent's combined clock/codec lease as its sole
clock ownership; it does not acquire the separate clock policy again. This is
still a software-only path, not a live backend. An IRQ delivered before DL1
start or after its synchronized stop is ignored instead of poisoning the
engine. This neither identifies nor acknowledges a pending hardware interrupt,
so IRQ routing and source quiescence must be established before connecting it.

The optional `A04_AUDIO_MAP_AFE=1` diagnostic PnP path also requires the
parent query. It identifies exact AFE/SRAM ranges in raw PnP descriptors,
checks paired translated lengths, exclusive sharing and non-overlap, maps the
translated AFE register page, and unmaps it on PnP revocation or
final release. It performs no register accesses, does not map SRAM, and
never connects the IRQ. Mapping a PnP resource is not a codec/clock lease or
proof of safe hardware ownership. This path must remain disabled if any other
driver or firmware can still touch AFE, and would require a new teardown
design before adding an ISR, DMA or any asynchronous mapping user.

The diagnostic PnP backend now gives the DL1 engine ordered 32-bit access to
the PnP-translated AFE mapping. The callbacks allow only DL1/DAC/IRQ offsets
used by the engine and deny writes to CUR and IRQ_STATUS. They are compiled
only with `A04_AUDIO_IRQ_DIAGNOSTIC=1`, which requires parent query and AFE
mapping; the mapping survives ISR disconnect and DPC rundown. The shared
parent still does not grant the required clock/codec lease or masked IRQ
contract, and backend RUN still rejects. The callbacks do not prove clock
power, live buffer coherency, the speaker route, or a DMA drain. Host tests
use simulated MMIO; there has been no Windows or device playback test.

`Mt8167TransportStartMaskedParent` handles the PnP case where the IRQ was
connected under the version 4 masked-source contract. It prepares the common
buffer, quarantines the lease before publishing a DMA address, and starts the
engine under IRQ synchronization without reconnecting the ISR. Failures after
the exposure boundary retain the mapping, IRQ and lease. This is host-tested
but is not wired to backend RUN: continuous cache synchronization, clock and
codec ownership, muted startup and a DMA drain remain unresolved.

`A04_AUDIO_RUN_DIAGNOSTIC=1` connects backend start/stop to the preconnected
parent transport, but `OwnerLiveDmaVerified` always rejects: one pre-publish
flush does not prove live WaveRT circular-buffer coherency. The current parent
also refuses codec/clock ownership. The hypothetical start/stop path is not
operational or hardware-tested; do not enable it for deployment. A successful
stop still does not prove drain or permit freeing exposed DMA.

Version 4 additionally distinguishes refusal before side effects (-1) from
uncertain partial acquisition (any other failure). The latter quarantines
the child lease/reference and causes the parent to retain mappings on release.
This prevents ordinary software cleanup from freeing uncertain resources; it
does not prove that Windows keeps the physical PnP resources assigned after
release or surprise removal. No hardware audio lease is currently enabled.

Diagnostic observation builds may set `A04_AUDIO_DIAGNOSTIC_TRACE=1` for the
parent and audio build checks. They log parent admission and callback presence,
RUN rejection, and AFE registers sampled under interrupt synchronization after
STOP. `A04_AUDIO_PMIC_TRACE=1` additionally reads TOP_CKPDN1 over the parent
PWRAP lock during the experimental D0 transition; failure rejects D0 and a
WACS timeout poisons the bus. Both flags default to zero. These observations
are not evidence of powered rails, clock ownership, codec quiescence, or DMA
drain. The real parent still lacks the callbacks required to acquire audio and
RUN remains unavailable; exposed DMA and the parent lease remain quarantined.

The current parent contract is version 5 and includes the child's explicit
AFE mapping bind/unbind. The diagnostic backend binds only its validated
PnP-assigned mapping before lease acquisition and retains that mapping if the
parent cannot release it. The parent still rejects real acquisition; this does
not establish DMA drain, board mute, power retention or hardware playback.

The current parent interface is version 6. Diagnostic stream stop requests a
codec mute without releasing power before disabling DL1 and disconnecting its
IRQ; even a mute failure does not skip the stop attempt. This path cannot run
on the real parent until the retained-power, exclusive ownership, codec and
live DMA prerequisites are established. DL1 enable readback is not DMA drain.

`A04_AUDIO_SILENT_DMA_DIAGNOSTIC=1` is an opt-in, silent DMA prototype: it
uses a private zero-filled hardware buffer and a separate PortCls buffer. It
does not transfer application PCM to the device and must not be described as
audio playback. The shared parent still refuses acquisition on hardware, and
the new path has only host-side checks; WDK and target execution remain
unverified. Any buffer exposed to DMA is retained because hardware drain has
not been established.

The PnP backend serializes passive-level operations with a kernel mutex rather
than FAST_MUTEX. AllocateCommonBuffer and FreeCommonBuffer require
PASSIVE_LEVEL, while FAST_MUTEX raises the caller to APC_LEVEL. Unexpected
higher-IRQL allocation or start is rejected; an unexpected high-IRQL free
retains the allocation. GetPosition remains protected by rundown and does not
take this blocking mutex. See [AllocateCommonBuffer](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pallocate_common_buffer)
and [FreeCommonBuffer](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pfree_common_buffer).

`A04_AUDIO_PCM_PATTERN_DIAGNOSTIC=1` selects an immutable test waveform in the
private diagnostic DMA ring instead of zeroes. It requires
`A04_AUDIO_SILENT_DMA_DIAGNOSTIC=1`, uses 48 kHz stereo S16_LE at 1500 Hz and
peak 128/32768, and never maps that DMA ring to the WaveRT client. The buffer
is filled before the existing pre-publication flush. The WaveRT client's PCM
is not rendered by this experiment; the codec remains muted. On exposure,
the DMA mapping and ownership remain quarantined because DL1 disable and IRQ
disconnect do not prove bus drain. Normal playback and all diagnostic flags
remain off by default. Parent lease, board power, codec route and physical
coherency still require separate establishment; no device run is claimed.

`A04_AUDIO_UNMUTE_DIAGNOSTIC=1` additionally requests MT6392 gain code 1
(Linux defines it as +0 dB, the lowest non-mute gain) only after the fixed PCM
DMA starts. It requires the fixed-pattern and isolated-DMA flags. The parent
requires an independently verified board route and retained power callback;
the actual parent does not provide it, so the real device still rejects this
operation. A failed request still stops DL1 and retains exposed DMA. This
experiment does not connect WaveRT PCM or prove the MT8167 analog route.
Linux's AIF RX, DAC, Vref, DAC clock, VCM and AU_LOL path must also be
implemented and verified before any unmute is admitted.

`voice_playback.h` is a host-tested experimental composition of the existing
MT8167 Voice Amp sequence with DMA and downstream mute operations. It requires
preflight to establish exclusive owners, actual voice-path calibration,
downstream mute and live PCM coherence before touching the codec. Voice starts
while muted, DMA starts next, and unmute follows only after both succeed. Stop
requests mute and then DMA stop even when mute fails; analog shutdown is allowed
only when both succeed and the codec helper verifies mute again. Any attempted
start permanently retains resources; DMA stop does not establish bus drain.
The callbacks have no production provider and this helper is not wired into PnP
or WaveRT. Host tests model ordering and failure paths, not physical playback.
Linux's `mt8167-codec.c` DAPM routes connect Left DAC through Voice Amp and
LINEOUT Mux to AU_LOL; this does not establish the board's downstream route or
prove headphone calibration applies to voice output.
