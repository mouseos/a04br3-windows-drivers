/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_TRIANGLE_SUITE_H
#define A04_M111_NATIVE_TRIANGLE_SUITE_H
#include "native_clear_job.h"
#define A04_M111_TRIANGLE_DATA_BYTES 0x4b0u
#define A04_M111_TRIANGLE_USC_BYTES 0x140u
#define A04_M111_TRIANGLE_PDS_BYTES 0x244u
struct a04_m111_triangle_record { uint8_t ta[A04_M111_TA_BYTES],three[A04_M111_3D_BYTES]; };
struct a04_m111_triangle_suite {
    struct a04_m111_triangle_record record[2];
    uint8_t data[A04_M111_TRIANGLE_DATA_BYTES];
    uint8_t usc[A04_M111_TRIANGLE_USC_BYTES],pds[A04_M111_TRIANGLE_PDS_BYTES];
};
/* Source-generated fixed800x600 solid then 1x1 texture triangle. Uses the
 * three already-backed 4KiB General/USC/PDS pages supplied by the cold owner.
 * memory.rgba is ignored: both backgrounds are explicitly opaque black.
 * Records reference only these owned spans and caller-owned HWRT/aux/output.
 * Cold HWRT/PM/aux initialization is the same policy as validated native clear.
 * Caller maps and verifies bytes before FW start and retains actual DMA.
 * CPU success never reports GPU completion. scratch is a distinct, 8-aligned
 * sizeof(suite) byte host buffer; avoids kernel-stack-sized resource shadows.
 * No allocation, device access, queue publication or capability advertisement. */
int a04_m111_build_triangle_suite(struct a04_m111_triangle_suite *out,
    const struct a04_m111_clear_job_input *memory,void *scratch,size_t scratch_bytes);
uint32_t a04_m111_triangle_expected_pixel(unsigned frame,unsigned pixel);
#endif
