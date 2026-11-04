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
acquisitions; D0 always fails and it publishes no interface or child. No INF or
ACPI binding is provided. The current button and touch providers already claim
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
sets the PWRAP owner offline under that same mutex before unmapping. D0 still
always fails and never starts the owner, so calls fail without MMIO. No production client,
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
documented lifetime and power contract. No online provider or child PDO currently
exists, and interface references alone cannot keep physical hardware present
or powered. The remaining local keypad and I2C accesses also need a verified
surprise-removal design. These compile-only clients are not installable builds.

The compile-only parent now publishes the input client interface as well as
its older offline diagnostic interface. The parent translates the public touch
bank IDs, dispatches narrow operations through the existing owners and uses one
blocking lock for GPIO/TOP/INFRA and PWRAP. D0 exit, surprise removal and
release close admission; stop waits for an operation already holding that lock.
The parent still never starts the owners and always rejects D0. A physical
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
parent D0 continues to fail. The source-level fixture checks cannot establish
that ACPI descriptors are exclusive on the target or that every firmware or
Windows driver has ceased bypassing the owner.

With `A04_SHARED_QUERY_EXPERIMENTAL=1`, the prototype additionally publishes a
separate versioned audio interface; its codec/clock acquire and release always
fail without MMIO. With `A04_SHARED_CHILD_PROBE=1`, a third experimental audio
child forwards that GUID to its parent. The child has no AFE resources, INF or
audio FDO binding and its requirements callback refuses to start. Parent D0
still fails. This does not establish ownership or codec safety.

The parent stop path closes both touch and PWRAP admission while holding their
single shared mutex, before releasing any mapping. Existing operations finish
before this lock can be acquired; no new operation can enter either owner after
it is released. This is a software rundown guarantee only: the prototype still
has no mechanism that prevents the physical device disappearing while MMIO is
already in progress, nor proof of retained rail or clock power. D0 remains
unconditionally refused and a stop must never be interpreted as power control.
