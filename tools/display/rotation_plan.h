/* SPDX-License-Identifier: MIT */
#ifndef MT8167_ROTATION_PLAN_H
#define MT8167_ROTATION_PLAN_H
#include <stdint.h>
/* DEVMODE DMDO values, not a sensor attitude or a display-driver control. */
static int mt8167_rotation_size(uint32_t width,uint32_t height,uint32_t original,
 uint32_t requested,uint32_t *w,uint32_t *h)
{
 uint32_t nw,nh;
 if(!w||!h||!width||!height||width>0x7fffffffu||height>0x7fffffffu||original>3||requested>3)return 0;
 nw=(original&1)?height:width;nh=(original&1)?width:height;
 *w=(requested&1)?nh:nw;*h=(requested&1)?nw:nh;return 1;
}
#endif
