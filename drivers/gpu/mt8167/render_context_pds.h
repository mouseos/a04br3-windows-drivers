/* SPDX-License-Identifier: MIT */
#ifndef A04_RENDER_CONTEXT_PDS_H
#define A04_RENDER_CONTEXT_PDS_H
#include "render_execution.h"
/* Fixed GE8300 128-byte persistent-state store/load PDS constants.
 * Output is generated from the owned GPU VA and includes zero padding.
 * No code, relocation, submission, or hardware validation is provided. */
struct a04_render_context_pt_data { uint8_t store[32],resume[32]; };
int a04_render_make_context_pt_data(uint64_t state_gpu,
    const struct a04_render_owned_range *ranges,size_t count,
    struct a04_render_context_pt_data *out);
/* Shared-register PDS constants reference a 16432-byte state allocation and
 * executable USC entry. Input ranges must belong to the caller and be held
 * through GPU completion and cleanup; this function does not submit work. */
int a04_render_make_context_sr_data(uint64_t state_gpu,uint64_t shader_gpu,uint64_t doutu_control,
    const struct a04_render_owned_range *ranges,size_t count,uint8_t out[64]);
#endif
