/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_PAIR_H
#define A04_M111_NATIVE_PAIR_H
#include "native_ccb.h"
#define A04_M111_PAIR_WORKSPACE_BYTES 8192u
struct a04_m111_shadow_ring { void *cpu; uint32_t read,write; };
struct a04_m111_pair_input {
    uint32_t ta_context,three_d_context,hwrt;
    uint32_t ta_sync,three_d_sync,ta_previous_value,three_d_update_value;
    uint32_t external_job,internal_job;
};
struct a04_m111_pair_result {
    uint32_t ta_write,three_d_write;
    RGXFWIF_KCCB_CMD ta_kick,three_d_kick;
};
/* Compose the combined-PR/full-3D KM path: TA, UPDATE; FENCE_PR, 3D,
 * UPDATE. There is no separate PR payload. Caller serializes access to both
 * ordinary CPU shadows. Both rings/result stay unchanged on failure; only
 * disposable workspace may change. No Device memory or MMIO publication.
 * Input payloads must be independently initialized complete records. This
 * transport builder checks proven shared fields, not shader correctness. */
int a04_m111_prepare_shadow_pair(const struct a04_m111_shadow_ring *ta_ring,
    const struct a04_m111_shadow_ring *three_d_ring,
    const struct a04_m111_pair_input *input,const void *ta_payload,
    const void *three_d_payload,void *workspace,size_t workspace_bytes,
    struct a04_m111_pair_result *result);
#endif
