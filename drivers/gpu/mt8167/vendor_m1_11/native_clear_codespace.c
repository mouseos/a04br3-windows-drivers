/* SPDX-License-Identifier: MIT
 * Background USC: source-compiled Mesa PCO, BVNC 22.40.54.30,
 * source base 0d94b3f397fb9cf8470bb5600ada8af79ff5361a.
 * load_preamble(0) -> frag_store_pco(output0), olchk.end, temp0/shared2.
 * Shared loader: Rogue DOUTW END, constant64 slot0/control32 slot2.
 * Layout is a host packing policy, not a vendor ABI allocation layout.
 */
#include "native_clear_codespace.h"
#include "native_clear_pds.h"
#include "native_eot_program.h"
static void le32(uint8_t *p,uint32_t v)
{ unsigned i;for(i=0;i<4;i++)p[i]=(uint8_t)(v>>(8u*i)); }
static int span(uint64_t va,uint64_t base,uint32_t bytes,uint32_t align)
{ return va<base || va-base>UINT64_C(0x40000000)-bytes || (va&(align-1u)); }
int a04_m111_build_clear_codespace(struct a04_m111_clear_codespace *out,
    uint64_t usc_gpu,uint64_t pds_gpu,uint32_t colour,uint32_t word0,uint32_t word1)
{
    static const uint8_t bg[16]={0x38,0x8a,0x80,0x87,0x80,0x08,0,0,
                               0,0x20,0xf3,0xff,0xff,0xff,0xff,0xff};
    struct a04_m111_clear_codespace made={{0},{0}};
    struct a04_m111_eot_program eot;unsigned i;
    if(!out || span(usc_gpu,UINT64_C(0xba00000000),sizeof(made.usc),4) ||
       span(pds_gpu,UINT64_C(0xa800000000),sizeof(made.pds),16))return 1;
    if(a04_m111_build_eot_program(&eot,usc_gpu+16u,word0,word1) ||
       a04_m111_patch_background_pds(made.pds,36,usc_gpu,0))return 1;
    for(i=0;i<16;i++){made.usc[i]=bg[i];made.usc[16+i]=eot.usc[i];}
    /* Write both shared registers; sh1 is unused by this background shader. */
    le32(made.pds+0x40,colour);le32(made.pds+0x48,0x90000002u);
    /* Unconditional DOUTW, end: shared-loader has no USC task. */
    le32(made.pds+0x50,0xf4020001u);
    for(i=0;i<sizeof(eot.pds);i++)made.pds[0x80+i]=eot.pds[i];
    *out=made;return 0;
}
