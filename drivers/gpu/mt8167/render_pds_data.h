/* SPDX-License-Identifier: MIT */
#ifndef A04_RENDER_PDS_DATA_H
#define A04_RENDER_PDS_DATA_H
#include "render_execution.h"
/* Metadata-only constructor for the two PDS layouts observed in the fixed
 * 64x64 GE8300 diagnostic. It contains no shader, command stream, submission
 * path or DMA operations and is not linked into a hardware driver by default.
 * Caller must establish the origin of the literal/control words, validate the
 * fixed shader bytes and retain every mapped range for the GPU lifetime. */
struct a04_render_pds_input {
    uint64_t shader_gpu, spill_info_gpu, scratch_info_gpu;
    uint64_t spill_buffer_gpu, scratch_buffer_gpu, doutu_control;
    uint32_t vertex_literal, descriptor_literal0, descriptor_literal1;
};
struct a04_render_pds_data {
    uint8_t vertex[12];
    uint8_t descriptor[24];
    uint8_t spill_info[12];
    uint8_t scratch_info[12];
};
/* Construct from GPU addresses, never host pointers. Required addresses must
 * fit the device VA width, belong to caller-owned ranges with appropriate
 * access, not overlap each other, and satisfy their alignment. Uses the
 * observed diagnostic's spill 8192/4 and scratch 49152/24 byte geometry.
 * On failure the output remains unchanged. */
int a04_render_make_pds_data(const struct a04_render_pds_input *input,
    const struct a04_render_owned_range *ranges,size_t range_count,
    struct a04_render_pds_data *out);
/* Fixed single-sample fragment kick: 32-byte data and 16-byte code allocation.
 * Only a tracked executable USC GPU address and DOUTU control bits are used;
 * all unused bytes are zero. Does not validate the shader or submit work. */
int a04_render_make_fragment_pds(uint64_t shader_gpu,uint64_t doutu_control,
    const struct a04_render_owned_range *ranges,size_t range_count,uint8_t out[48]);
/* Fixed probe: vertex ID DOUTW, DOUTU, HALT; and two descriptor DOUTDs.
 * Sixteen-byte allocations include zero padding. These encode no addresses. */
void a04_render_make_vertex_pds_code(uint8_t vertex[16],uint8_t descriptor[16]);
#endif
