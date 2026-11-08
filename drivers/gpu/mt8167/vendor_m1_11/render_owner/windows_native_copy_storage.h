/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_NATIVE_COPY_STORAGE_H
#define A04_WINDOWS_NATIVE_COPY_STORAGE_H
#include "windows_native_cold.h"
#include "native_copy_job.h"
struct a04_native_copy_storage;
struct geo_surface_desc;
/* Factory-private cold storage for a real RT -> staging transfer. Caller
 * retains native owner until release succeeds and serializes graph lifetime.
 * No arbitrary GPU address supplied: derive both VAs from current native
 * registered bindings, check actual PTEs against their WDDM physical page lists.
 * Copy code is the existing identity-verified152B DOD transfer asset only.
 * A returned nonnull object on failure MUST be retained if release fails. */
NTSTATUS a04_native_copy_storage_create(struct a04_native_copy_storage **,
 struct a04_vendor_native_cold *,struct a04_native_mdl_binding *staging,
 const struct geo_surface_desc *actual_staging_description,
 const void *copy_usc,ULONG copy_usc_bytes);
NTSTATUS a04_native_copy_storage_release_cold(struct a04_native_copy_storage **);
/* CPU-only task image; hardware publication must separately hold actual GPU
 * cache ownership for both MDLs and keep this storage pinned through retirement. */
const struct a04_native_copy_job_image *a04_native_copy_storage_image(
 const struct a04_native_copy_storage *);
struct a04_vendor_native_cold *a04_native_copy_storage_parent(const struct a04_native_copy_storage *);
/* Internal transfer-only ownership half, not a scheduler completion API. */
NTSTATUS a04_native_copy_storage_cpu_complete(struct a04_native_copy_storage *);
BOOLEAN a04_native_copy_storage_gpu_owned(const struct a04_native_copy_storage *);
#endif
