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
