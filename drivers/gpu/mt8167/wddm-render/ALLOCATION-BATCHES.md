# Allocation DDI batches

This change adds bounded allocation metadata transactions; it is not evidence
that run232's inactive display path is caused by allocation batching or fixed.
No feature, format, source-address flags, native public ops, wire ABI or EDK2
contract is expanded.

## Supported requests and admission

All four DDIs accept 1..256 entries. Destroy retains the existing zero-entry
DestroyResource-only path. Array/count checks precede dereferences and products;
transaction storage is nonpaged pool, not a large kernel-stack array.
Duplicate input handles and duplicate translated allocations in one Open are
rejected. Multiple independent opens of one allocation remain possible; Close
can close those distinct handles together after checking their aggregate count.

The existing shared-surface wire defines one immutable allocation/resource.
Its single Create/Destroy and resource-only destruction are retained. Multi
Create/Destroy with that shared wire is not supported and is rejected before
publication or destruction; the change does not invent a multi-allocation
resource wire. Ordinary allocations and primary-v2 use their existing validators.
No new acceptance of SubresourceIndex or flag bits is introduced.

## Create: prepare, admit, publish

The frontend session's PASSIVE lock/rundown covers the entire DDI. Every private
descriptor is validated and copied before creating any allocation node. Frontend
nodes are allocated but unpublished. The source-linked native helper then
validates every descriptor and prepares unpublished cold resource nodes.

The private helper accepts only this native factory's create/destroy function
pointers and ABI4 ops size/version/context. It never treats another provider's
opaque context as an e_native_owner. It acquires owner rundown/mutex once and
checks active/D0/leased/closing and the complete monotonic-ID range. Failure
frees only unpublished pool objects, does not consume IDs, write output handles,
change hResource, or require a fallible destroy rollback. Once admitted, native
list publication/ID assignment and then frontend list/output publication have
no further fallible operation. The frontend session prevents another PASSIVE
DDI from observing intermediate frontend state; no exposed new handle exists
before publication. Native rundown covers the native commit.

The single Create path retains its original owner.create call. Single and multi
share the original frontend descriptor validation and allocation-info output
fields. The native single/multi descriptor checks also share one implementation
with the original status distinction preserved.

## Destroy: validate all, commit without fallible callbacks

Multi Destroy validates all frontend handles, uniqueness, count, and resource
ownership. Under the real adapter IRQ synchronization it checks engine/allocation
poison, bound state, job references, open references, and display references for
EVERY allocation. Any display-backed/bound multi request returns busy without
Restore, drain, Unmap, ref decrement or other display mutation. The single display
retirement path is unchanged.

All-zero references exclude submitted or captured jobs using these allocations.
The held frontend session excludes new opens, maps and captures between admission
and commit. The native private helper independently takes rundown/mutex once,
resolves every unique token, and rejects any remaining native linear/binding
ownership before unlinking anything. No owner.destroy loop is used. After full
admission, native unlink/free cannot return a later-element error. Frontend
unlink/count/free follows with no fallible operation. The existing void
GeoEndVidMmMap cleanup remains PASSIVE and follows native retirement, as in
single Destroy; no mapping is newly created by the transaction.

## Open and Close

Open resolves every OS allocation handle through the actual Dxgk callback and
validates membership/private metadata before preparing every per-device open
object. Under actual IRQ synchronization it admits all poison/overflow checks,
then increments all open references. Only then are the device list and output
handles published. Any earlier failure frees unpublished objects only.

Close validates all handles against the requesting device (not another device),
rejects duplicates, and checks all jobs and aggregate open counts in one actual
IRQ synchronization callback. It preserves the existing captured state4 close
rule. Only after ALL checks pass are all counts decremented; the PASSIVE caller
then unlinks/frees the already validated open nodes. A late busy/invalid element
cannot close earlier ones. No pool or native callback occurs inside IRQ locking.
Session modes/IRQL/rundown remain the existing admission mechanism.

## Validation and qualifications

Run `python3 drivers/gpu/mt8167/wddm-render/tests/allocation_batches.py --output
<new-project-root-directory>`. The fixture extracts and executes the actual four
DDIs, private native batch helpers, native single create/destroy and engine single
create/destroy. It uses the actual descriptor validators. Pool/IRQ/session/
rundown/display and Windows structure definitions are host models; this is NOT
Windows ABI, real concurrency or hardware verification.

One combined fixture batch covers late invalid descriptors/handles, all prepare
pool failure points, duplicate handles/tokens, count/reference/ID overflow, late
native busy/admission failures, wrong-device Close, same-allocation aggregate
Close, display multi no-mutation, single primary/shared lifetime, and 256-entry
transactions for all four DDIs. ASan/UBSan and pool/reference/list assertions are
used. A Windows WDK compile/link, signature, deployment and actual display
activation are not performed by this source change.

A future build needs the changed kmd_clear.c, kmd_objects.c, and
native_owner_factory.c translation units plus this new private header. Retained
objects containing these old implementations cannot implement the new helper
symbols. No native/PCO cache guard or source-plan guard is relaxed by this change.
