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
exposes only the fixed MT6392 button-status read under a parent-owned WDFWAITLOCK.
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

Admission is closed atomically before D0 exit, surprise-removal or release
waits for the shared mutex. Every GPIO and PWRAP operation rechecks the closed
flag after acquiring that mutex, so a caller already queued on it cannot enter
behind stop. Surprise removal also latches removal permanently for that device
object: PrepareHardware rejects before mapping and checks again after mapping.
Normal prepare/reprepare still does not open admission, and D0 remains refused.
These checks prevent software reuse but do not make an in-flight bus access safe
if power or hardware disappears before the notification.

The optional eMMC query interface exposes only the existing fixed raw-rail
observation under the same PWRAP owner. It returns failure with output unchanged
while admission is closed. It does not grant a rail, clock, pin, card or bus
lease, change hardware, publish a child, or connect Storport; an object
reference is not a power reference. Existing audio acquire remains fail-closed.
Actual rail and clock control, card reinitialization, audio codec sequencing,
PnP topology and Sx resume remain prerequisites for any operational connection.

`A04_SHARED_EMMC_CHILD_PROBE=1` adds a fourth synthetic child and forwards
its eMMC query to the real parent stack. The read-only INF in `prototype/`
matches only that child. Both parent D0 and the eMMC child's resource query
still reject startup: current firmware exposes the controller as ACPI
`MOU0001`, and simply reporting the same address would compete with that
owner. The original ACPI interrupt has level/active-low attributes; a software
child's `IO_RESOURCE_DESCRIPTOR` uses bus-relative vector requirements and
cannot be assumed to reproduce that wiring or translation. No MMIO range or
interrupt is assigned to this child and no miniport can bind successfully.
Firmware must select a mutually exclusive topology and a verified Windows 8.1
interrupt/resource contract must be implemented before this changes.

A physical Storport miniport must use the port/miniport contract; obtaining a
PDO does not by itself authorize it to issue arbitrary WDM IRPs or call KMDF.
The intended boundary is a real parent that keeps shared rails/clocks/pins
owned through child power transitions and enumerates a dedicated controller
PDO, with Storport receiving its private resources from PnP. The existing
rail query is for an appropriate PASSIVE-level WDM/KMDF client and does not
supply a working miniport connection. References:
https://learn.microsoft.com/en-us/windows-hardware/drivers/storage/overview-of-storage-virtual-miniport-drivers
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_io_resource_descriptor

`vemc_restore` is an unconnected PASSIVE-level experiment implementing the
MT6392 WACS2 protocol for one narrow transition: VEMC enable 0->1, preserving
its selector and never changing VIO18. It requires an already-held exclusive,
non-revocable parent lease, all rail consumers quiescent, a separately trusted
selector policy, a monotonic deadline and a 264-us minimum ramp wait. It checks
VIO18 and selector before writing, then re-reads all three registers. Failure
poisons the state and output remains unchanged; a failed command may still have
reached the PMIC and must not be retried or rolled back blindly. Tests simulate
WACS2 and cannot demonstrate real PMIC behavior, exclusive ownership, physical
rail voltage or bus-abort safety. There is no caller from the WDF parent, D0,
Storport or eMMC code; it must remain so until those guarantees exist.

An alternative compile-only `A04_SHARED_EMMC_FILTER_PROBE=1` uses a KMDF lower
filter in the existing ACPI eMMC device stack instead of synthesizing an IRQ
on a new bus. Its exclusive ACPI controller range is recognized but never
mapped by the filter; four separate shared ranges are mapped. Resource lists
and IRQ are not modified. A board-only firmware option supplies the controller
range first and preserves the existing ACPI active-low GIC descriptor unchanged.
The filter still refuses D0 and has no operational power/clock lease; its INF is
an offline fixture with demand-start services, not a boot package. On Windows it
remains unverified whether Storport receives the intended range and IRQ, or
whether this topology can meet boot, rundown, power and other-client ownership
requirements. Do not install or deploy it.

The parent wait lock is created as a device child and is acquired only at
PASSIVE_LEVEL. Stop closes admission before waiting for the current holder;
queued callers recheck admission after acquisition. This does not make an
in-flight MMIO access safe against physical removal. D0 stays gated, and the
eMMC preparation prototype is not connected to this lock or to Storport.

The prototype pairs each raw descriptor with its translated descriptor by the
PnP list index. Raw memory addresses identify the board resources; only the
translated physical addresses are mapped, after length, exclusivity and alias
checks. The filter validates that both interrupt descriptors are level and
exclusive, and leaves the OS-translated IRQ untouched for Storport. This is not
proof of the Windows interrupt polarity, translated vector, power ordering or
safe live removal. Both D0 and the miniport startup gate remain closed.
See https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/raw-and-translated-resources
