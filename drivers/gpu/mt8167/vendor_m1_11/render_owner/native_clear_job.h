/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_CLEAR_JOB_H
#define A04_M111_NATIVE_CLEAR_JOB_H
#include "native_clear_codespace.h"
#include "native_clear_data.h"
#include "native_record_fields.h"
struct a04_m111_clear_job_input {
    uint64_t output_gpu,data_page_gpu,usc_page_gpu,pds_page_gpu;
    uint64_t dummy_region_gpu,dummy_stencil_gpu;
    uint32_t hwrt_fw,pr_fence_fw;
    uint8_t rgba[4];
};
struct a04_m111_clear_job {
    uint8_t ta[A04_M111_TA_BYTES],three[A04_M111_3D_BYTES];
    struct a04_m111_clear_codespace code;
    struct a04_m111_clear_data data;
};
/* GE8300 22.40.54.30 / m1.11: first cold 800x600 linear RGBA8 clear,
 * pitch800, one RT, no MSAA/Z/S/streamout/queries/overlap. Frame and PR
 * fence initial value0. Not a repeat-frame, resize, draw or general D3D API.
 * Inputs are Windows-owned mapped addresses, NOT captured Android VAs.
 * Builds both complete record byte arrays and generated USC/PDS/data shadows.
 * Owner supplies initialized cold HWRT/freelists/auxiliary/output and copies
 * shadows with barriers before using existing TA/3D queue lifetime rules.
 * No allocation, device access, submission, or GPU-success claim here.
 * A successful CPU return is not authorization to open the execution gate.
 * In particular, unread auxiliary contents are not certified by this API.
 * Success changes out only, error leaves it unchanged; out/in must not alias.
 * Code and data use separate owned 4KiB pages in their respective heaps. */
int a04_m111_build_clear_job_800x600_cold(struct a04_m111_clear_job *out,
    const struct a04_m111_clear_job_input *input);
#endif
