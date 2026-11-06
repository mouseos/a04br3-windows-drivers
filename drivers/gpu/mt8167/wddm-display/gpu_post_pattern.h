/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GPU_POST_PATTERN_H
#define A04_GPU_POST_PATTERN_H
#include <stdint.h>
/* Source data is copied by the GPU; the CPU verifies every output pixel. */
static inline uint32_t a04_gpu_post_pixel(uint32_t x,uint32_t y)
{
    static const uint32_t colors[8]={0xffff3030u,0xffffa020u,0xffffff20u,0xff20e040u,
                                    0xff20e0e0u,0xff3060ffu,0xffd040e0u,0xffeeeeeeu};
    if ((x>=80u && x<720u && (y==320u || y==959u)) ||
        (y>=320u && y<960u && (x==80u || x==719u)))return 0xff000000u;
    return colors[(x/100u)&7u];
}
#endif
