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
No interface, binding, INF, direct-MMIO fallback or release of the existing D0
and eMMC startup gates is provided here.
