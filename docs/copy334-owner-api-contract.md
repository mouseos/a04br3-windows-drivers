# copy334 owner API freeze

Integration branch owner: Mencius (this worker), `integrate/copy334-wddm-aperture`. Merge shared caller changes only in this branch; do not simultaneously edit the same caller files in two trees. Huygens owns copy/wire/IRQ/retire logic and supplies its forward delta. Mencius owns `native_owner_mdl.[ch]` and caller ownership/cache adaptation, including the shared `windows_native_target/execute/storage`, `native_owner_factory` and `windows_native_copy_storage/transfer` files. Main owns final integration evaluation, media and hardware. Neither this branch nor the signed SYS is a hardware success claim.

## Frozen contract

- Exact WDDM1.x MAP_APERTURE physical MDL frontend provenance/lease is required by bind. Never use arbitrary physical addresses, packet-DMA logical SG addresses or user MDLs. `pages()` remains valid through resident lease; CPU ownership does not revoke PTEs.
- `sync_for_gpu`: PASSIVE, serialized native owner, previous real fence/CPU ownership required. Clean exact locked MDL, set GPU ownership for exposed backing. Already GPU-owned returns DEVICE_BUSY **before** cache work. Initial cold sync leaves CPU accessible until `mark_exposed`.
- `mark_exposed`: once before root publication; closes CPU data access. `gpu_owned()` is an explicit retained/exposed/live-PTE/GPU-owned predicate; NULL `cpu()` is not used as ownership proof. `cpu_identity()` is comparison-only, never I/O.
- `sync_for_cpu`: PASSIVE, serialized owner; caller supplies real task+SLC+GP completion or complete parent-off/IRQ proof. This API cannot manufacture that proof. Returns cache ownership, not scheduler completion.
- `retire_after_gpu_off`: only complete real parent OFF/reset/IRQ rundown path; then remove child mappings, drop holds and release physical frontend lease in existing order. IRQ receipt, idle, mask or a timeout alone does not permit this call.

## Minimal existing runtime callers

The following fragments use actual private functions; they are ordering examples, not a new standalone driver entry point. Full error handling retains/quarantines any exposed graph.

```c
/* Cold graph, PASSIVE, before FW root publication. Bind and attach APIs
 * already retain exact frontend MDL references and program verified PTEs. */
status = a04_vendor_native_target_before_fw_start(native);
if (!NT_SUCCESS(status)) return status; /* no publication on error */
/* Existing factory starts FW only after this succeeds. */
```

```c
/* Next render, PASSIVE/native mutex, after previous real finish returned
 * CPU ownership. All RT/VB/linear callers must transition together. */
status = a04_vendor_native_mdl_gpu_prepare(native);
if (!NT_SUCCESS(status)) return status;
/* Existing publish is under actual Dxgk IRQ synchronization. */
```

```c
/* Copy cold storage + transfer preparation occur before FW starts.
 * Both MDLs belong to the parent's retained graph. */
status = a04_native_copy_storage_create(&storage, native, staging,
                                       &staging_desc, copy_usc, copy_bytes);
if (!NT_SUCCESS(status)) return status; /* retain nonnull failed graph */
status = a04_native_tq_transfer_prepare(&transfer, storage);
/* Parent target_before_fw_start closes CPU access for both MDLs.
 * Existing IRQ-locked submit verifies storage_gpu_owned before any write.
 * begin_finish records only the actual locked ISR receipt. */
```

```c
/* PASSIVE worker; actual ISR receipt was captured under IRQ lock and
 * graph/rundown remain retained. Existing finish proves TQ/UFO/SLC/GP. */
status = a04_native_tq_transfer_finish_gpu(transfer, start, &result);
if (!NT_SUCCESS(status)) return status; /* poison/retain, no fence */
/* result.phase == 4: exact source+destination cache CPU-owned.
 * command_busy remains retained. NO Dxgk completion is issued here. */
```

## Remaining consumer boundary (Huygens)

WDDM frontend invocation, wire command validation/serialization, fresh visibility IRQ→scheduler fence, command_busy release, submitted-transfer retirement and child-before-parent release must be connected in Huygens runtime. The cold transfer object is one-shot: state4 is not a second submit permission. No new reset or hardware trial is authorized by this contract. These are concrete remaining runtime connections, not implemented by a signing fixture. The existing native render rearm and copy completion/cache/ownership predicate are actually implemented on this integration branch.
