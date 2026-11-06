# MT8167 shared resources

`pwrap_owner` serializes a narrow MT6392 button-status transaction through a
parent-supplied blocking lock. It enforces stop and preserves timeout poison;
it does not publish an interface or claim resources. A real Windows PnP parent
must exclusively own/map PWRAP, initialize and retain the lock and power,
manage client references and teardown, and stop firmware/other drivers from
bypassing it. The existing button WDF provider still maps PWRAP itself; do not
run a separate owner concurrently. Clock ownership, shared clock leases,
rate arbitration and PMIC audio writes are not implemented.

The button core offers an explicit shared callback path. When selected, a
missing callback fails and cannot fall back to direct access. It is not wired
to Windows PnP. Legacy direct access remains unchanged until a real parent
and a verified migration can be deployed together.

`clock_owner.c` is a locked lease policy for a future PnP parent. It requires
a parent-provided clock verification callback, refuses incompatible rates and
new leases after stop, and poisons the policy if verification fails. It neither
claims clock registers nor changes PLLs, gates or rates. It cannot establish
that firmware and other Windows drivers have stopped bypassing the owner; it is
not connected to any device.

The narrow eMMC rail observation reads three fixed PMIC registers under the
same owner lock as button reads and publishes results only if all complete.
It does not enable rails or change voltage. The parent/provider and safe PnP
lifetime are still unimplemented; no current client may bypass the existing
button driver's direct PWRAP ownership to use this core.

`wdf_parent_probe.c` is a non-installable KMDF PnP ABI prototype. It validates
four exclusive translated memory ranges (TOPCKGEN, INFRA, GPIO and PWRAP)
before mapping and unmaps partial
acquisitions. D0 rejects by default; optional diagnostic interfaces and child
PDOs are described below. The prototype INF and ACPI fixture are not a production binding. The current button and touch providers already claim
parts of these ranges. Exclusive descriptors alone do not prove that firmware
or existing direct owners have been migrated. Boot ordering, shared leases,
client teardown, rail/pin/clock operations and actual Storport integration are
not implemented. Never deploy this prototype alongside those drivers.

`register_owner` and the touch `shared_board` adapter are host-tested policy
components, not a Windows interface. They limit touch register operations and
serialize each operation under a parent-supplied lock; I2C remains client-owned.
They do not hold a lifetime reference, atomically reserve the entire touch
initialization sequence, or connect the existing WDF driver. The eMMC UHS pin
profile similarly requires an exclusive powered, quiescent parent and readback;
it is not invoked by the boot driver. All partial failures must leave the
affected state unavailable until a verified recovery, not retry blindly.

The existing ACPI touch and button devices are separate stacks. A query from
one does not automatically reach an unrelated shared owner. A future bus must
establish actual child PDOs and forward their query to the parent stack, with
client reference and removal ordering, before replacing the direct mappings.
See Microsoft documentation for [WDF_QUERY_INTERFACE_CONFIG](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfqueryinterface/ns-wdfqueryinterface-_wdf_query_interface_config)
and [WdfFdoQueryForInterface](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdffdo/nf-wdffdo-wdffdoqueryforinterface).
No production interface, binding, INF, direct-MMIO fallback or release of the existing D0
and eMMC startup gates is provided here.

The parent probe optionally publishes a versioned, reference-counted kernel
query interface when `A04_SHARED_QUERY_EXPERIMENTAL=1`; the interface currently
exposes only the fixed MT6392 button-status read under a parent-owned FAST_MUTEX.
The mutex and callbacks exist before interface publication, and ReleaseHardware
sets the PWRAP owner offline under that same mutex before unmapping. Default D0 rejects and leaves the owner offline, so calls fail without MMIO. No production client,
INF, ACPI parent/child topology, audio clock lease or PMIC audio operation is
connected. WDF interface references retain the device object but do not make its
hardware resources active. Windows PnP behavior and transition to an online
owner remain unverified; never deploy next to existing direct PWRAP users.

The button and touch WDF sources have opt-in client paths selected at build
time by `A04_BUTTON_SHARED_PARENT=1` and `A04_TOUCH_SHARED_PARENT=1`.
Their build scripts produce a SYS for compile-only checking and deliberately
skip INF and catalog packaging in these modes. Button expects only keypad MMIO;
touch expects only I2C0 MMIO. Each obtains the prototype query interface in
PrepareHardware, stops and drains its timer before dereferencing in Release,
and fails rather than mapping shared ranges when the interface is absent.
These clients require actual child stacks and a provider that maintains the
documented lifetime and power contract. No operational online provider or
validated enumerated child PDO currently exists, and interface references alone cannot keep physical hardware present
or powered. The remaining local keypad and I2C accesses also need a verified
surprise-removal design. These compile-only clients are not installable builds.

The compile-only parent now publishes the input client interface as well as
its older offline diagnostic interface. The parent translates the public touch
bank IDs, dispatches narrow operations through the existing owners and uses one
blocking lock for GPIO/TOP/INFRA and PWRAP. D0 exit, surprise removal and
release close admission; stop waits for an operation already holding that lock.
Default D0 rejects and leaves both owners offline. A physical
removal concurrent with an MMIO operation can still cause a bus abort, which a
software lock cannot prevent. Do not infer safe removal or power ownership.

The compile-only `A04_SHARED_CHILD_PROBE` creates two input and one audio static synthetic PDOs and
forwards input interface queries to their actual parent stack. These PDOs have
probe-only IDs and advertise exclusive fixed keypad (0x10002000) and I2C0
(0x11009000) resource requirements. PnP assignment and translation of these
requirements have not been tested on Windows. `prototype/` contains INF and
ACPI fixtures for offline validation only: the SSDT does not disable the legacy
MOU0003/MOU0004 ACPI devices and is not included in firmware or packaging.
Never install it alongside them. The INF files require shared-client builds and
have not been signed or validated on a device. There is no active power owner,
no verified firmware resource migration, and no physical-removal guarantee;
default parent D0 continues to fail. The source-level fixture checks cannot establish
that ACPI descriptors are exclusive on the target or that every firmware or
Windows driver has ceased bypassing the owner.

With `A04_SHARED_QUERY_EXPERIMENTAL=1`, the prototype additionally publishes a
separate versioned audio interface; its codec/clock acquire and release always
fail without MMIO. With `A04_SHARED_CHILD_PROBE=1`, a third experimental audio
child forwards that GUID to its parent. The child has no AFE resources, INF or
audio FDO binding and its requirements callback refuses to start. Default parent D0
still fails. This does not establish ownership or codec safety.

The parent stop path closes both touch and PWRAP admission while holding their
single shared mutex, before releasing any mapping. Existing operations finish
before this lock can be acquired; no new operation can enter either owner after
it is released. This is a software rundown guarantee only: the prototype still
has no mechanism that prevents the physical device disappearing while MMIO is
already in progress, nor proof of retained rail or clock power. Default D0 remains refused and a stop must never be interpreted as power control.

`A04_AUDIO_CHILD_RESOURCES=1` is a separate, default-off diagnostic resource
requirements prototype for the audio child. It proposes exclusive AFE MMIO
0x11140000+0x1000, SRAM 0x11141000+0x9000, and a level-sensitive raw interrupt
candidate 152 (DTS SPI 120 + GIC SPI base 32). The IO_RESOURCE_DESCRIPTOR
cannot encode the DTS active-low polarity; Windows raw/translated IRQ mapping,
controller configuration, competing legacy devices, and PnP arbitration have
not been verified. Parent D0 and audio codec/clock callbacks still refuse to
start. No audio INF or firmware binding is supplied. This is not permission to
install or to connect an interrupt; the default child remains resource-less.

`a04_pwrap_owner_read_audio` reads the fixed MT6392 `SPK_CON0`, `SPK_CON9`
and `SPK_CON11` registers (0x0052, 0x0064, 0x0068) in one locked WACS2
transaction sequence, shared with button and eMMC observations. These
addresses are from the board kernel's [MT6392 register definitions](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/include/linux/mfd/mt6392/registers.h).
Output is unchanged on a partial failure and WACS timeouts poison the owner.
This is a raw, read-only diagnostic helper, not a proof of mute, ownership,
rail state or trim. It is not published as a client interface or invoked by
D0/audio acquisition; the parent and RUN gates remain closed.

The offline parent pairs raw and translated PnP descriptors by resource-list
index: raw addresses identify TOPCKGEN, INFRA, GPIO and PWRAP, while only the
exclusive non-overlapping translated ranges are mapped. See Microsoft's
[raw and translated resource documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/raw-and-translated-resources).
This follows the resource-pairing and admission work in commits `6bd1cf6` and
`53a6c02`, adapted here to preserve the audio v2 query and existing children.
The admission gate is closed before waiting for an active operation on stop;
queued callers recheck after locking. The offline AML fixture has `_STA=0` so
it cannot enumerate alongside the legacy button fixture. This is not an
operational mutual-exclusion configuration: the parent still rejects D0, and a
board-specific mutually exclusive enumeration and power handoff remain required.

`A04_SHARED_BUTTON_D0_EXPERIMENT=1` additionally requires the query and child
flags. After exclusive PnP resources are mapped it reads PWRAP INIT_DONE2 and
WACS2_RDATA, and admits only the button PWRAP owner when initialized and idle.
A stop closes admission before draining the shared mutex; surprise removal also
prevents a later D0 restart. Readiness does not prove retained power or firmware
non-interference. Touch register and audio owners stay offline. The separate
TAB-A04-BR3 firmware branch `windows-rt/shared-acpi-owner` selects the shared
parent instead of legacy button and touch devices only with an explicit build
option; the default remains legacy. Neither Windows arbitration, removal nor
the diagnostic configuration has been verified on hardware. Do not deploy it
alongside direct PWRAP users.

The shared audio interface now invokes `audio_owner` under the same lock as
PWRAP and the shared register owner. Its serialized contract orders exclusive
clock acquisition before a muted codec and releases the codec before the clock,
only after the caller independently proves DMA and IRQ quiescence. Any
uncertain hardware operation poisons the owner instead of attempting speculative
rollback. Stop quarantines an outstanding lease. The platform callbacks and
online transition are deliberately absent, so the real parent still rejects
all audio leases without hardware access. The lifecycle of mappings and parent
references on a quarantined lease must be implemented before enabling them.
Linux MT8167 APLL selection and MT6392 speaker sequences are evidence for
dependencies, not proof of exclusive Windows ownership or safe board gain.

`A04_SHARED_APMIXED_RESOURCE=1` is a separate default-off PnP resource
experiment. The parent requires five exclusive raw/translated memory pairs,
including the MT8167 DTS APMIXED syscon `0x10018000+0x710`, validates all
translated ranges against overlap, maps the translated APMIXED range and
unmaps it during normal rundown. No APMIXED registers are accessed. The normal
four-resource configuration remains unchanged and rejects the extra range.
The codec DTS references APMIXED, AFE, TOP audio and PWRAP; APMIXED is shared
with other users including the Linux thermal driver. The default ACPI configuration does not provide this fifth resource. The
separate canonical UEFI branch `work/windows-shared-parent-acpi` offers it
only with `A04BR3_SHARED_PARENT=1` and `A04BR3_SHARED_APMIXED=1`,
and rejects concurrent thermal, GPU, display and battery build selections. Windows PnP exclusivity does not prove
that firmware or drivers using direct MMIO have stopped, nor does mapping
prove clock rates, rail retention, codec mute, or a safe DMA drain. The audio
lease and all clock/codec writes remain disabled. Do not deploy this experiment.
Sources: [MT8167 DTS](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/arch/arm64/boot/dts/mediatek/mt8167.dtsi),
[A04BR3 DTS](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/arch/arm64/boot/dts/mediatek/a04br3.dts).

`A04_SHARED_MT6392_DIAGNOSTIC=1` compiles a Class D codec start/stop
implementation and binds it to the parent's existing audio owner under the
PWRAP lock. It sets the Linux-defined gain code 0 (MUTE), checks readback,
performs the board kernel's hardware-trim register sequence with 20 ms and
2 ms waits, and implements its Class D stop sequence after verified quiescence.
Any failed or ambiguous operation poisons the codec and performs no automatic
rollback. Power, bus ownership, board-level mute, SET/CLR write semantics,
clock and DMA/IRQ quiescence require platform callbacks. Those callbacks and
the parent audio online transition are deliberately absent: even with the
flag, the parent rejects before accessing hardware. A failed multi-step stop
may already have changed some registers; it does not prove physical mute.
Reference: [board Linux MT6392 codec](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/sound/soc/codecs/mt6392-codec.c).

`A04_MT6392_WRITE_DIAGNOSTIC=1` adds a bounded WACS2 read-modify-write
implementation for only the exact Class D SPK register masks and values. It
serializes under the parent's existing PWRAP lock, verifies the complete
readback and poisons the shared bus on timeout or mismatch; the diagnostic
codec callbacks use it. The codec checks support for every sequence operation before its first write.
TOP_CKPDN1_CLR is explicitly rejected before any bus operation: alias semantics and safe sharing are not established, and the
parent still has no actual power/clock/board-mute preflight. Therefore the
Class D path must not run on hardware. Existing direct button builds do not
include the diagnostic writer. Linux [WACS2 write encoding](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/drivers/soc/mediatek/mtk-pmic-wrap.c) is the source of the command format. Readback of SPK registers and bus idle do not establish safe rails or DMA drain.

For MT6392, the board kernel names `TOP_CKPDN1`/`_SET`/`_CLR` at
0x0108/0x010a/0x010c. Its Class D driver uses `snd_soc_update_bits` on the
CLR alias with mask 0x000e and value 0x000e at start, but with value 0 at
stop; the codec routes that access through the PWRAP regmap. These sources
do not specify alias readback, effective write-zero behavior, which other
consumers require those clock bits, or retained power during Windows D0 exit.
A generic RMW or guessed SET write is therefore not a safe implementation.
The Class D implementation now checks both its complete start and stop
capabilities before the first hardware write. A partially failed clock/codec
acquisition is returned as an uncertain outcome in audio interface v4; the
child quarantines its parent reference and the parent refuses to unmap on
release. Windows may still reclaim resources after PnP release or physical
removal, so this is not a physical power or exclusivity guarantee.
Sources: [registers](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/include/linux/mfd/mt6392/registers.h),
[MT6392 codec](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/sound/soc/codecs/mt6392-codec.c),
and [MT8167 codec regmap](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/sound/soc/codecs/mt8167-codec.c).

The board kernel's `snd_soc_update_bits` routes via an uncached MT8167 codec
regmap to PWRAP: it reads the alias, computes `(old & ~mask) | (value & mask)`,
and writes the alias only if changed. Its codec-regmap lock callbacks are
empty. This establishes Linux software behavior, not the MT6392 hardware's
alias readback or write-zero semantics. The board `upmu_hw.h` identifies
TOP_CKPDN1 bits 1, 2 and 3 as speaker PWM divider, divider and speaker clock
power-down fields. `a04_pwrap_owner_read_speaker_clocks` now observes the raw
base register 0x0108 under the same mutex as button and codec transactions.
It is not exposed to a client and no current path invokes it on hardware;
it cannot establish exclusive ownership or permission to change the clocks.
The Class D parent callback now uses a bounded kernel wait for the Linux
2 ms/20 ms delays. Preflight, alias semantics, clock ownership, power and
quiescence remain unresolved and the audio lease remains offline.
Sources: [ASoC](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/sound/soc/soc-io.c),
[regmap](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/drivers/base/regmap/regmap.c),
and [board PMIC fields](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/drivers/misc/mediatek/include/mt-plat/mt8167/include/mach/upmu_hw.h).

`A04_MT6392_CLK_ALIAS_EXPERIMENT=1` adds a default-off diagnostic
interpretation of the MT6392 speaker clock aliases under the parent's same
PWRAP lock. It snapshots TOP_CKPDN1, writes the previously set speaker PDN
bits to CLR, and requires a base-register readback with those bits cleared
and unrelated bits unchanged. After independently verified quiescence it
writes only the originally set bits to SET and verifies restoration. A failed
write/readback poisons the bus and retains the clock lease; it does not retry
or attempt speculative rollback. Parent release refuses to unmap with such a
lease. This SET-based restore is a diagnostic ownership strategy, not a claim
that Linux executes the same stop operation: Linux performs update_bits on CLR
with zero. The alias hardware behavior, retained power, other consumers and
DMA drain have not been proven. The parent audio preflight, clock provider and
codec quiescence callbacks are still absent, so the actual parent cannot
execute the sequence or start audio even when all diagnostic flags are set.
Host tests model alias behavior; they do not prove it on the device.

The owner now requires an independent `verify_release` callback before it
will acquire a lease and calls it before any codec shutdown or clock release.
A failed verification retains the active lease without issuing hardware writes;
a callback must prove DMA and interrupt quiescence rather than merely trust
the child, a disabled bit, or an unchanged DMA pointer. The actual parent
does not provide this callback and remains offline. The board Linux
`mt8167-afe-util.c` enables `top_pdn_aud` before the reference-counted AFE
gate and disables them in reverse order; its shared clock framework and
`afe_ctrl_lock` are not reproduced by a PnP mapping. The available Windows
resources and source do not yet prove equivalent shared ownership or drain.
Source: [MT8167 AFE clock handling](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/sound/soc/mediatek/mt8167/mt8167-afe-util.c).

`A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC=1` binds the shared parent's TOPCKGEN
clock lease into the audio-owner sequence. MT8167 Linux DTS connects
`top_pdn_audio` to `CLK_TOP_AUDIO`; the board clock driver uses status 0x24,
CLR 0x84, SET 0x54, bit 25. Acquisition saves the existing gate state, clears
only that bit when needed and verifies readback. Release runs after codec and
AFE clock shutdown, checks that the gate is still enabled and restores only
its original state. Failure poisons and retains ownership. Host mock tests do
not validate the physical register or external owners. The real parent still
lacks power/AFE ownership, codec prerequisites and DMA/IRQ drain proof; this
flag alone cannot put the audio owner online or execute the gate operation.
For a device diagnostic, first prove firmware and all direct TOPCKGEN/PWRAP
clients have handed over, PnP resource exclusivity and D0 retention, AFE bit 2
ownership with its child mapping, codec muted power and IRQ/DMA quiescence;
then verify readback and stop behavior under the platform owner's control.
No such device verification has been performed.
Sources: [DTS](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/arch/arm64/boot/dts/mediatek/mt8167.dtsi),
[clock driver](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/drivers/clk/mediatek/clk-mt8167.c),
[clock gate operations](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/drivers/clk/mediatek/clk-gate.c).

The audio QUERY_INTERFACE is version 5. The diagnostic audio child supplies
its validated exclusive PnP AFE mapping to `BindAfe` before acquiring the
lease, and keeps it mapped until unbinding succeeds. Binding serializes with
parent rundown, rejects a second mapping and performs no MMIO; it is a
lifetime contract, not proof against firmware or uncoordinated direct users.
`A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC=1` adds parent callbacks for the Linux
`AUDIO_TOP_CON0` AFE PDN bit 2, after the TOP audio gate. They preserve the
initial bit, verify writes, and retain the mapping and poison state on mismatch.
A single parent mutex serializes its clients but cannot establish ownership by
itself. The diagnostic flag requires the TOP gate flag and remains OFF by
default. The parent still has no platform preflight, powered-rail guarantee,
codec board-mute verification or independent DMA/IRQ quiescence callback, so
it cannot grant a lease and these callbacks cannot run on a device yet.
Linux clears DL1 enable and masks/clears IRQ at STOP, but the available Linux
sources do not specify that outstanding bus transactions have drained. Neither
this sequence nor a stationary cursor permits freeing DMA memory. The Windows
backend retains exposed DMA and the parent lease; device removal may still
revoke hardware resources, so this is not a complete PnP removal guarantee.

The audio interface is now version 6. It adds a mute-only operation that does
not release DMA, clock or power and may be called at APC_LEVEL while the audio
backend mutex is held. On a running stream the backend asks the parent to mute
before stopping DL1; it still attempts DL1/IRQ stop if mute fails and keeps
quarantined resources. Failure to write or verify the MT6392 SPK_CON9 mute
field poisons the codec and owner. Codec startup now reads VAUD22 enable/QI,
VAUD22 selector and VAUD28 enable/QI through the serialized PWRAP owner, and
rejects startup if either enable or QI is clear. The board Linux DTS marks
both regulators always-on and the regulator driver identifies bit 14 as enable
and bit 15 as QI. This is a necessary register check, not an electrical
measurement, rail retention guarantee, or evidence of exclusive ownership.
The parent still does not grant a real lease and live DMA RUN remains refused.
Sources: [MT6392 DTS](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/arch/arm64/boot/dts/mediatek/mt6392.dtsi),
[regulator driver](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/drivers/regulator/mt6392-regulator.c),
[IRQ handler](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/sound/soc/mediatek/mt8167/mt8167-afe-pcm.c).

The integration branch combines the audio and USB ownership prototypes under
the same parent lock and admission state. USB admission is possible only in
the existing opt-in D0 experiment with the optional 0x710-byte APMIXED resource;
the USB owner must still verify its clocks and overcurrent before VBUS. Normal
builds keep parent D0 disabled. The ACPI table is an offline fixture with
`_STA=0`; legacy owners, firmware handoff, PnP removal and physical power
sharing have not been validated. Host tests and WDK compilation do not prove
that USB or audio operates on the tablet.

For a shared-resource diagnostic, `A04_SHARED_TOUCH_D0_EXPERIMENT=1` admits
the existing BU21025 child register interface only after the guarded parent
D0 entry; it defaults to zero and requires the guarded D0 experiment. Stop
closes admission and takes the same lock before turning it off. This requires
the mutually exclusive shared-parent ACPI configuration and the shared-touch
driver build; an installed legacy touch provider must not coexist. Host mocks
check admission and shutdown, but target touch, suspend/resume and ownership
have not yet been validated.

The diagnostic parent has also been linked as an unsigned ARM32 SYS using
the WDK 8.1 import libraries. This is a build check only: the offline ACPI
fixture, USB keyboard child and audio backend have not been installed or
exercised together on the target.

`A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC=1` adds the common Linux MT8167
codec clock/reference sequence under the parent lock: CLKLDO (`CON01` bit 20),
codec clock (`CON03` bit 30), DL_CLK (`CON04` bit 15), and both Vcm14 bits (`CON00` bits 19 and 1).
The Class D preflight, rails and exclusive APMIXED ownership must be satisfied
before these writes; the parent currently has no qualifying preflight and
therefore cannot execute the sequence. Initial bit states are saved and
restored in reverse order only after Class D quiescence; readback mismatch
poisons and retains the lease. This is not a complete DAC-to-AU_LOL route:
Linux also needs DAC, Vref/VCM, voice amp and
DC compensation/ramp. The updated ACPI diagnostic resource provides 0x714
bytes; legacy 0x710 resources still cannot access offset 0x710. Proving
calibration and shared thermal/codec ownership remains necessary; the gated codec-domain path requires the extended resource before any writes.

APMIXED compatibility: the parent accepts legacy 0x710 and extended 0x714
resources, mapping and unmapping exactly the assigned size. Only 0x714 includes
the 32-bit Linux AUDIO_CODEC_CON04 at offset 0x710. Older parent builds reject
the extended resource; update the diagnostic firmware and driver together.
The default four-resource configuration remains unchanged. A mapping is not
proof of ownership. The gated codec-domain path enables DL_CLK only after
preflight and refuses a legacy-length mapping before any domain writes.

The gated codec domain also acquires Linux DAPM supplies DL_Vref24
(CON02 bit 16), DAC_CLK (CON02 bit 27), DL_VCM1 (CON01 bit 13) and
DL_VCM2 (CON02 bit 17), saving initial bits and restoring in reverse.
It does not enable the Left DAC or Voice Amp: Linux performs calibrated DC
compensation and ramp around DAC transitions, and the parent does not yet
own the ABB AFE registers or have validated board calibration.

The standalone left-channel DC ramp helper follows the Linux ABB CON3/CON11
coefficient and latch sequence, including capacitor-dependent step and delay.
It is not yet wired to the PnP backend: no validated calibration provider or
proof of muted output and exclusive AFE ownership is available. No zero
calibration value is assumed. The Linux LINEOUT mux is a virtual DAPM mux;
Voice Amp uses CON02 bit 8 with gain bits 12:9, and Left DAC uses CON01 bit 15.

The board Linux calibration measures headphone AUXADC voltage at VCM, DAC,
and amplifier stages; it derives DAC-to-amplifier and amplifier-to-VCM
coefficients per headphone PGA gain. `codec_calibration.h` only converts two
measured microvolt values with those Linux coefficients, rejecting overflow;
it does not acquire measurements or establish their applicability to the
voice/line-out path. Linux sets the initial VCM-to-DAC coefficient to 0x4b4b,
not a measured per-device value. The Linux AUXADC helper labeled Cali converts
raw samples to microvolts and explicitly says there is no voltage calibration.
The implementation remains disconnected from hardware pending a validated
measurement provider, path equivalence, exclusive ownership, muted startup and
PnP teardown. These calculations do not prove audible or safe playback.
The ramp helper is not a complete DAC or playback path.

The isolated `codec_voice.h` experiment models the board Linux AIF RX, left
DAC DC ramp and voice amplifier depop ordering. It requires a caller to prove
both downstream mute and Linux-compatible per-device calibration before any
write; no caller currently supplies that proof. It is not connected to the
parent, AUXADC, or PCM and cannot produce audio. Its callback is an interface,
not proof that headphone calibration is appropriate for AU_LOL. The initial DAC-stage coefficient and ramp endpoint are supplied by the
verified calibration provider; the Linux utility can calculate 0x4b4b but
its invocation is not established for every startup. Failure retains the active state and
poisons the experiment, requiring explicit recovery. Host tests simulate the
registers only and do not validate analog behavior or actual hardware.

The default-off `A04_SHARED_VOICE_DIAGNOSTIC` connects the prototype to the
parent: diagnostic DMA starts before Voice Amp and minimum Class D unmute;
stop mutes Class D before stopping DMA, and verified quiescent release stops
Voice Amp before its supplies. `ParentVoiceVerify` unconditionally rejects:
there is no proven AU_LOL calibration, capacitance or downstream mute provider.
The existing parent preflight and independent DMA/IRQ drain proof are also
unavailable. This is a buildable sequencing candidate, not a playback driver.
Linux measures 22 headphone ADC samples, discards a minimum and maximum, and
computes B-to-C per headphone gain. Its 0x4b4b is 500 mV times 38.55, not a
measured DAC endpoint; measured headphone OP compensation is not evidence of
AU_LOL equivalence. Calibration may be requested through Linux HP DC Offsets
control; neither that control nor its put path proves the value was measured
on this boot. No per-device data is embedded here.

Linux allocates the codec private data with `devm_kzalloc`, so both DAC
compensation arrays initially contain zero. `mt8167_codec_gather_comp_val`
sets VCM-to-DAC entries to 0x4b4b only after an HP DC Offsets get or put;
the latter can use supplied rather than freshly measured offsets. The left
DAC DAPM event uses those arrays for both headphone and Voice Amp routes.
The Voice helper therefore accepts the initial value as well as the target
from its provider, and retains both through stop. This does not establish
that headphone measurements characterize AU_LOL: the parent still rejects
the operation and does not claim an AUXADC lease. Ramp timing uses CON02
bits 20:19, read from hardware in Linux, rather than assuming the DTS default.
already in progress, nor proof of retained rail or clock power. D0 remains
unconditionally refused and a stop must never be interpreted as power control.

The experimental query interface now includes a Wi-Fi ownership ABI. Its
Acquire and ReleaseQuiescent callbacks always reject, and the parent still
rejects D0. With the existing child-probe build flag the parent creates a
separate `A04BR3\MT6630SDIO` PDO, gives it an exclusive MSDC2 memory
requirement, and forwards its Wi-Fi interface queries to the parent. The
existing ACPI Wi-Fi candidate remains independent and must not be enabled
alongside this child. No IRQ is claimed: translating the MSDC2 interrupt and
supplying EINT5 still require a real interrupt provider. The PDO has no bound
NDIS driver, the lease cannot be acquired and this is not a radio test.

The optional version 8 audio interface wires the MT8167 Voice Amp helper to
parent-owned APMIXED and the child's bound AFE mapping. The diagnostic parent
still rejects PCM coherency verification and the voice calibration/mute check;
no unverified board assumptions can activate it. It uses the same shared lock
and requires the existing codec domain and lease. A partial voice sequence
poisons the owner; its resources must remain retained. The PCM client and
parent flags default off and do not establish PnP or hardware safety.

The legacy Voice diagnostic and PCM diagnostic share one Voice state and the
same calibrated initial/end values. With both flags enabled, PCM explicitly
starts the Voice path before DMA; the legacy unmute callback recognizes the
active path instead of starting it twice. Legacy-only mode retains its
DMA-then-Voice ordering. Both remain blocked by the real parent's absent mute
and calibration verification; the PCM path also lacks a DMA mapping and drain
proof. Wi-Fi/RTC flags are independent and are exercised with both enabled in
the host and ARM32 build checks.
