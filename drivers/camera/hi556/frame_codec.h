/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_HI556_FRAME_CODEC_H
#define A04_HI556_FRAME_CODEC_H
#include <stddef.h>
#include <stdint.h>
#include "capture.h"
/* MT8167 IMGO: four consecutive little-endian 10bit samples per five bytes.
 * This is NOT CSI-2/MIPI RAW10 packing. GBRG order is CT3 HI556 RAW_Gb.
 * Standard output averages the two actual greens in each 2x2 Bayer cell;
 * no guessed gain, OTP, black level, color matrix or synthetic pixels. */
#define A04_HI556_RGB_WIDTH (A04_HI556_WIDTH/2u)
#define A04_HI556_RGB_HEIGHT (A04_HI556_HEIGHT/2u)
#define A04_HI556_RGB_STRIDE (A04_HI556_RGB_WIDTH*3u)
#define A04_HI556_RGB_BYTES (A04_HI556_RGB_STRIDE*A04_HI556_RGB_HEIGHT)
struct a04_hi556_content {unsigned minimum,maximum,nonzero,nonconstant;};
int a04_hi556_content_check(const uint8_t *,size_t,struct a04_hi556_content *);
int a04_hi556_rgb24(const uint8_t *,size_t,uint8_t *,size_t);
#endif
