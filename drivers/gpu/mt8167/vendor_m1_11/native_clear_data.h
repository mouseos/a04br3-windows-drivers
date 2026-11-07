/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_CLEAR_DATA_H
#define A04_M111_NATIVE_CLEAR_DATA_H
#include <stdint.h>
#define A04_M111_CLEAR_DATA_BYTES 160u
#define A04_M111_CLEAR_SCISSOR_OFFSET 128u
#define A04_M111_CLEAR_DBIAS_OFFSET 144u
struct a04_m111_clear_data {
    uint8_t bytes[A04_M111_CLEAR_DATA_BYTES];
    uint64_t vdm_gpu,ppp_gpu,scissor_gpu,dbias_gpu;
};
/* One cold General-heap 4KiB page: proven no-streamout VDM/PPP terminator,
 * two full-extent scissor entries and zero depth-bias entry.
 * Padding is host packing policy, never a substitute for an unread resource.
 * Copy only bytes[] to GPU memory; the following addresses are host metadata.
 * This does not build or publish a complete TA/3D record. */
int a04_m111_build_clear_data(struct a04_m111_clear_data *out,
    uint64_t page_gpu,uint32_t width,uint32_t height);
#endif
