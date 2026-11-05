/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * CPU-only native22 texture/PDS data. This is not a transfer task and must
 * never be submitted; the matching fragment USC and complete task are open.
 */
#ifndef A04_VENDOR_NATIVE_COPY_STATE_H
#define A04_VENDOR_NATIVE_COPY_STATE_H
#include <stdint.h>
#include <stddef.h>
struct a04_vendor_copy_state_input {
    uint64_t source_va, destination_va, shared_va;
};
struct a04_vendor_copy_state {
    uint32_t shared[9], pds_data[4], pds_code[4];
};
/* One 32x32, pitch32, native22 surface per disjoint 4 KiB scratch range.
 * VA bounds/overlap validation here does not establish page mappings,
 * ownership, cache coherency, fragment-code identity, or GPU completion.
 * The original producer compares allocation mapping bases; disjoint VAs
 * here do not establish the independent-mapping branch. */
int a04_vendor_copy_state_build(struct a04_vendor_copy_state *out,
    const struct a04_vendor_copy_state_input *input);
/* Partial shader-branch fields only: never a complete 208-byte task. */
struct a04_vendor_copy_kick_input {
    uint32_t fragment_offset, secondary_data_offset, secondary_code_offset, usc_offset;
};
struct a04_vendor_copy_kick_image {
    uint32_t pds_fragment[16];
    uint32_t raw00;
    uint64_t raw38,raw40,raw48,raw50;
};
int a04_vendor_copy_kick_build(struct a04_vendor_copy_kick_image *out,
    const struct a04_vendor_copy_kick_input *input);
#endif
