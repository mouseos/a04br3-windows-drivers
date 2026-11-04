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
