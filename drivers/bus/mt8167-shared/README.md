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
