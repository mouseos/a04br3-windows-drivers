/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_TRIANGLE_VDM_H
#define A04_M111_NATIVE_TRIANGLE_VDM_H
#include <stddef.h>
#include <stdint.h>
#define A04_M111_TRIANGLE_VDM_BYTES 84u
/* Outer stream only. Caller must provide independently built and backed PPP,
 * PDS, USC, vertex and texture resources. Does not admit or submit a GPU job.
 * This fixed capture05 profile contains a 2-index task before the triangle;
 * its shader role is deliberately not inferred from the topology alone. */
struct a04_m111_triangle_vdm_input {
    uint64_t prefix_ppp,draw_ppp,terminate_ppp;
    uint32_t draw_ppp_words; /* 26 solid, 27 sampled-texture profile */
    uint32_t first_pds_data,first_pds_code,vertex_pds_data,vertex_pds_code;
};
int a04_m111_build_triangle_vdm(void *out,size_t bytes,
    const struct a04_m111_triangle_vdm_input *input);
#endif
