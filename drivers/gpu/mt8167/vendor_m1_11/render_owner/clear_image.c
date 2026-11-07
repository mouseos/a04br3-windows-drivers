/* SPDX-License-Identifier: MIT */
#include "clear_image.h"
#include "render_resources.h"
#include <string.h>
int a04_vendor_clear_image_build(struct a04_vendor_clear_image *out,uint64_t target_va,uint32_t packed_colour)
{
    struct a04_vendor_clear_image image;uint64_t csr0,csr1;
    unsigned char *usc;
    if(!out || ((uintptr_t)out&7u) || target_va!=A04_VENDOR_RENDER_SCRATCH_VA || packed_colour==0xa5a5a5a5u)return 1;
    memset(&image,0,sizeof(image));
    image.expected=packed_colour;
    image.payload[2]=packed_colour;
    image.payload[1]=(~((8u<<10)-1u))^0xfffffffcu;
    image.payload[6]=2u|(2u<<16);
    image.payload[8]=0xa0000u|(3u<<12);
    image.payload[10]=((64u+480u)&0x7e00u)|2u;
    image.payload[11]=32u;
    image.payload[24]=3u;
    csr0=(UINT64_C(2)<<56)|(UINT64_C(30)<<33)|(UINT64_C(1)<<23)|(UINT64_C(2)<<26)|(UINT64_C(3)<<29);
    csr1=(UINT64_C(31)<<32)|31u;
    image.payload[32]=(uint32_t)csr0;image.payload[33]=(uint32_t)(csr0>>32);
    image.payload[34]=(uint32_t)csr1;image.payload[35]=(uint32_t)(csr1>>32);
    image.pds[2]=(uint32_t)(target_va>>2);
    image.pds[3]=(uint32_t)(target_va>>34)|(1u<<19);
    image.pds[4]=(1u<<31)|(1u<<28)|2u;
    image.pds[8]=(15u<<28)|(4u<<16)|(1u<<8)|1u;
    image.pds[9]=(15u<<28)|(1u<<26)|2u;
    usc=(unsigned char *)image.usc;
    usc[0]=0x46;usc[1]=0xa0;usc[2]=0x80;usc[3]=0xc2;
    usc[4]=0x80;usc[5]=0x40;usc[6]=0x80;usc[7]=0x01;
    usc[8]=0x88;usc[9]=0;usc[10]=0;usc[11]=0xff;
    *out=image;return 0;
}
