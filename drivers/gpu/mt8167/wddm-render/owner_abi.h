/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_OWNER_ABI_H
#define GEO_OWNER_ABI_H
#include "clear_protocol.h"
/* Kernel-only ops v2 proposal (control v2 is separate). The provider implements these operations; the
 * sidecar does not implement registers, firmware, shader or triangle runtime.
 * Reference/dereference, D0 and revoke/rundown are supplied by the binding.
 * All tokens are private kernel handles, never supplied by a user process. */
#define GEO_OWNER_OPS_VERSION 3u
#define GEO_OWNER_REGISTERED_MDL 1u
#define GEO_OWNER_PACKED_CLEAR 2u
#define GEO_OWNER_PREPARE_PASSIVE 4u
#define GEO_OWNER_SUBMIT_DISPATCH 8u
#define GEO_OWNER_COMPLETION_DIRQL 16u
#define GEO_OWNER_NORMAL_CPU_VISIBLE 32u
#define GEO_OWNER_REQUIRED 63u
#define GEO_COMPLETION_GPU_DONE 1u
#define GEO_COMPLETION_CPU_VISIBLE 2u
struct geo_owner_ops {
 uint32_t size,version,capabilities,reserved;
 void *context;
 /* PASSIVE: metadata creation; bind exactly these VidMm-owned locked pages.
  * bind retains the MDL/pages and validates usable Normal CPU memory plus
  * cache maintenance. A separate private output ring is forbidden. */
 int32_t (*create)(void *,const struct geo_surface_desc *,uint64_t *surface);
 /* Current native full-MDL profile: offset=0, MDL byte count equals
  * round_up(bytes,PAGE_SIZE). bytes is the active image extent; native SG/GPU
  * mapping covers the full rounded MDL, including padding. No partial MDL.
  * aperture_byte_offset is VidMm OffsetInPages*PAGE_SIZE, relative to the
  * actual GPU aperture base reported by this miniport. Redirect THIS range. */
 int32_t (*bind)(void *,uint64_t surface,void *locked_mdl,uint32_t mdl_byte_offset,uint32_t bytes,uint64_t aperture_byte_offset);
 int32_t (*unbind)(void *,uint64_t surface);
 int32_t (*destroy)(void *,uint64_t surface);
 /* PASSIVE: allocate/map/build/flush everything before scheduler submission. */
 int32_t (*prepare)(void *,uint64_t surface,uint32_t packed_argb,uint64_t *job);
 int32_t (*discard_prepared)(void *,uint64_t job);
 /* At the real interrupt synchronization IRQL: bounded publish only, no allocation, mutex, wait or polling.
  * Success means accepted, never GPU completion. Completion must not reenter
  * the consumer while this call is in progress. */
 int32_t (*submit)(void *,uint64_t job,uint32_t scheduler_fence,uint32_t consumer_generation);
 int32_t (*prepare_copy)(void *,uint64_t source,uint64_t destination,uint64_t *job);
};
/* Completion delivery must be coordinated with DxgkDdiInterruptRoutine at
 * the real DIRQL; DxgkCbNotifyInterrupt must not be called from a polling DPC.
 * The provider reports GPU_DONE|CPU_VISIBLE only after both are established.
 * Release after fault/removal is refused until real quiescence is provided. */
#endif
