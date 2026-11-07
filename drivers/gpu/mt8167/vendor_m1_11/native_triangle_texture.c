/* SPDX-License-Identifier: MIT
 * Layout: Imagination MIT rogue/texstate.xml (2022), corroborated by
 * original-driver capture05 ev_7266a0cd35c075c4598512415d1d4037ec0695ab2eb8926eb5ef84b821ff7c4b.
 * Descriptor pointers are not embedded: the caller regenerates PDS DMA data.
 */
#include "native_triangle_texture.h"
static void qword(uint8_t *p,uint64_t v)
{ for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(v>>(i*8)); }
int a04_m111_build_triangle_texture(struct a04_m111_triangle_texture *out,
    uint64_t gpu,const uint8_t rgba[4])
{
    struct a04_m111_triangle_texture made={0};
    if(!out || !rgba || gpu<4096u || gpu>(UINT64_C(1)<<40)-4u || (gpu&3u))return 1;
    /* SAMPLER_WORD0: clamp U/V=2, nearest min/mag=0, maxLOD=1/64,
     * minLOD=0, captured DADJUST encoding0xfff. WORD1 has no planar refs. */
    qword(made.sampler,(UINT64_C(2)<<44)|(UINT64_C(2)<<41)|(UINT64_C(1)<<23)|0xfffu);
    /* IMAGE_WORD0: U8U8U8U8=12, dimensions encoded0, 2D=1,
     * channel swizzles0/1/2/3; IMAGE_WORD1 alpha-MSB, 1level, VA>>2. */
    qword(made.image,(UINT64_C(12)<<27)|(UINT64_C(1)<<11)|(UINT64_C(2)<<8)|(UINT64_C(3)<<5)|1u);
    qword(made.image+8,(UINT64_C(1)<<59)|((gpu>>2)<<16)|1u);
    for(unsigned i=0;i<4;i++)made.texel[i]=rgba[i];
    *out=made;return 0;
}
