/* SPDX-License-Identifier: MIT
 * Board GLES PBE producer 0x20ff6c, format helper 0x22c9c0:
 * row at 0x26ac44 = 22 00 00 00 01 00 00 00 01 02 03 00.
 * One plane, packmode0, norm0, swizzle0/1/2/3. Layout3 table at 0x265080
 * gives 0x0200000000000000. Rogue PBE source_format bit19 selects packed
 * 8-bit input from output0; source_pos=0. Native CSR count is TWO, not three.
 * Extent and pitch CSR formula also matches proven native22 wide-state.
 */
#include "native_clear_surface.h"
int a04_m111_build_clear_surface(struct a04_m111_clear_surface *out,
    uint64_t output,uint32_t width,uint32_t height,uint32_t pitch)
{
    struct a04_m111_clear_surface made;uint64_t bytes;
    if(!out || !width || !height || width>16368u || height>16368u ||
       pitch<width || pitch>16384u || (pitch&31u) || output<4096u ||
       (output&127u) || output>=(UINT64_C(1)<<40))return 1;
    bytes=(uint64_t)pitch*height*4u;
    if(bytes>(UINT64_C(1)<<40)-output)return 1;
    made.emit[0]=(uint32_t)(output>>2);
    made.emit[1]=(uint32_t)(output>>34)|0x80000u;
    made.csr[0]=UINT64_C(0x0200000068800000)|
        (((uint64_t)(pitch-1u)&UINT64_C(0x1fffe))<<33);
    made.csr[1]=((uint64_t)(width-1u)<<32)|(height-1u);
    *out=made;return 0;
}
