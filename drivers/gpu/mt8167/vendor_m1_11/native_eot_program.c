/* SPDX-License-Identifier: MIT
 * USC is generated from the MIT-licensed Imagination Mesa PCO compiler,
 * source base 0d94b3f397fb9cf8470bb5600ada8af79ff5361a, BVNC 22.40.54.30:
 * nir_emitpix_pco(load_preamble(0),load_preamble(1)), freep=true, return.
 * PDS layout/encoding: board GLES FUN_00202600, DMA count 1; task control
 * FUN_002025c4. PCO emits a 16-byte-aligned final group; this is not a copy
 * of a complete vendor render packet. */
#include "native_eot_program.h"
#include "native_clear_pds.h"
static void le32(uint8_t *p,uint32_t value)
{ unsigned i;for(i=0;i<4;i++)p[i]=(uint8_t)(value>>(8u*i)); }
int a04_m111_build_eot_program(struct a04_m111_eot_program *out,
    uint64_t usc_gpu,uint32_t word0,uint32_t word1)
{
    struct a04_m111_eot_program made={{0},{0},16,8,2,0,2};unsigned i;
    static const uint8_t code[16]={0x48,0xa0,0x80,0xc2,0x80,0x40,0x80,0x01,
                                  0x88,0,0,0xff,0xf2,0xff,0xff,0xff};
    if(!out || a04_m111_build_eot_task_data(made.pds,8,usc_gpu,0))return 1;
    for(i=0;i<sizeof(code);i++)made.usc[i]=code[i];
    le32(made.pds+8,word0);le32(made.pds+12,word1);
    le32(made.pds+16,0x90000002u); /* two dwords -> shared registers 0/1 */
    /* Data words 5..7 are alignment padding, zero by host policy. */
    le32(made.pds+32,0xf0040101u);le32(made.pds+36,0xf4000002u);
    *out=made;return 0;
}
