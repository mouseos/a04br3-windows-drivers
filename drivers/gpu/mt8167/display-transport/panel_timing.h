/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_PANEL_TIMING_H
#define A04_PANEL_TIMING_H
#include <stdint.h>
/* DIS0.DTM0: sixteen little-endian DWORDs, no pointers or packed data.
 * Reserved DWORDs must be zero. Firmware must publish flags=0 unless this
 * boot supplied a valid
 * LK atag,videolfb for the identified panel. No nominal-rate fallback. */
#define A04_PANEL_TIMING_MAGIC 0x314d5444u
#define A04_PANEL_TIMING_VERSION 1u
#define A04_PANEL_TIMING_LK_VALID 1u
#define A04_PANEL_AUO_WXGA_DSI 1u
struct a04_panel_timing {
 uint32_t magic,version,size,flags,panel_id,width,height,htotal,vtotal,fps100;
 uint32_t fps_denominator,reserved[5];
};
struct a04_panel_frequencies {
 uint32_t vnum,vden,hnum,hden;
 uint64_t pixel_rate;
};
static inline uint32_t a04_timing_gcd(uint32_t a,uint32_t b) {
 while(b){uint32_t r=a%b;a=b;b=r;}return a;
}
static inline int a04_panel_timing_frequencies(const struct a04_panel_timing *t,
 struct a04_panel_frequencies *f) {
 uint32_t g;uint64_t h,p;
 if(!t||!f||sizeof(*t)!=64||t->magic!=A04_PANEL_TIMING_MAGIC||
    t->version!=1||t->size!=64||t->flags!=A04_PANEL_TIMING_LK_VALID||
    t->panel_id!=A04_PANEL_AUO_WXGA_DSI||t->width!=800||t->height!=1280||
    t->htotal!=960||t->vtotal!=1300||t->fps100<4500||t->fps100>7500||t->fps_denominator!=100||
    t->reserved[0]||t->reserved[1]||t->reserved[2]||t->reserved[3]||t->reserved[4])return 0;
 /* AUO vendor porches, NOT PLL245: 800+4+132+24,1280+4+8+8.
  * Reduce before multiplying to retain exact Hz and avoid 32-bit overflow.
  * PixelRate is pixels/s, not the DSI PLL or lane bit rate. */
 g=a04_timing_gcd(t->fps100,100);f->vnum=t->fps100/g;f->vden=100/g;
 g=a04_timing_gcd(t->vtotal,f->vden);h=(uint64_t)(t->vtotal/g)*f->vnum;
 if(h>=0xfffffffeu)return 0;
 f->hnum=(uint32_t)h;f->hden=f->vden/g;
 p=(uint64_t)t->htotal*f->hnum;
 if(p%f->hden)return 0; /* Never invent a rounded pixel clock. */
 f->pixel_rate=p/f->hden;
 return f->vnum<0xfffffffeu && f->pixel_rate>0;
}
#endif
