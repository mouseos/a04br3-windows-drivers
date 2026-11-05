/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native m1.11 source descriptor/PDS from board REA 12d0bc/12d28c/13eeac,
 * 13ca3c/13c794 and XNU A04BR3RogueNative22State.h. CPU image only.
 */
#include "native_copy_state.h"
#include "render_resources.h"
static int overlaps(uint64_t a,uint64_t an,uint64_t b,uint64_t bn)
{ return a<=b ? b-a<an : a-b<bn; }
static int in_scratch(uint64_t addr,uint64_t bytes,uint64_t align)
{
    const uint64_t start=A04_VENDOR_RENDER_SCRATCH_VA, size=131072u;
    return addr>=start && addr-start<=size && bytes<=size-(addr-start) && !(addr&(align-1));
}
int a04_vendor_copy_state_build(struct a04_vendor_copy_state *out,
    const struct a04_vendor_copy_state_input *input)
{
    struct a04_vendor_copy_state_input in;struct a04_vendor_copy_state image={0};
    uint64_t t0,t1;uintptr_t a=(uintptr_t)out,b=(uintptr_t)input;
    if(!out || !input || (a&3u) || (b&7u) || a>UINTPTR_MAX-sizeof(*out) || b>UINTPTR_MAX-sizeof(*input) ||
       (a<=b ? b-a<sizeof(*out) : a-b<sizeof(*input)))return 1;
    in=*input;
    if(!in_scratch(in.source_va,4096,128) || !in_scratch(in.destination_va,4096,128) ||
       !in_scratch(in.shared_va,36,16) || overlaps(in.source_va,4096,in.destination_va,4096) ||
       overlaps(in.source_va,4096,in.shared_va,36) || overlaps(in.destination_va,4096,in.shared_va,36))return 1;
    /* Native22, linear 32x32, pitch32, point sample, one plane. */
    image.shared[0]=0;image.shared[1]=0x2400;
    t0=(UINT64_C(12)<<27)|(UINT64_C(1)<<11)|(UINT64_C(2)<<8)|(UINT64_C(3)<<5)|4|
       (UINT64_C(31)<<34)|(UINT64_C(31)<<48);
    t1=(in.source_va<<14)|31|(UINT64_C(1)<<60)|(UINT64_C(1)<<59);
    image.shared[4]=(uint32_t)t0;image.shared[5]=(uint32_t)(t0>>32);
    image.shared[6]=(uint32_t)t1;image.shared[7]=(uint32_t)(t1>>32);image.shared[8]=255;
    image.pds_data[0]=(uint32_t)in.shared_va;image.pds_data[1]=(uint32_t)(in.shared_va>>32);
    image.pds_data[2]=0xf0000009u;
    image.pds_code[0]=0xc1400002u;image.pds_code[1]=0xf4020000u;image.pds_code[2]=0xd3000000u;
    *out=image;return 0;
}

/* Fixed native key8 metadata: 152-byte fragment, four USC temps, no sample
 * rate override; secondary first padded count 0, second 4, shared 9 and PDS
 * temps 1. FUN_129f88 packs these fields; FUN_12c568 makes p168==p170 when
 * first count is zero. Input offsets are within the private 64 KiB heaps.
 * Values alone never establish USC identity, mappings or a complete task. */
int a04_vendor_copy_kick_build(struct a04_vendor_copy_kick_image *out,
    const struct a04_vendor_copy_kick_input *input)
{
    struct a04_vendor_copy_kick_input in;struct a04_vendor_copy_kick_image image={0};
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)input;
    if(!out || !input || (a&7u) || (b&3u) || a>UINTPTR_MAX-sizeof(*out) || b>UINTPTR_MAX-sizeof(*input) ||
       (a<=b ? b-a<sizeof(*out) : a-b<sizeof(*input)))return 1;
    in=*input;
    if((in.fragment_offset&15u) || (in.secondary_data_offset&15u) || (in.secondary_code_offset&15u) ||
       (in.usc_offset&3u) || in.fragment_offset>65536u-64u ||
       in.secondary_data_offset>65536u-16u || in.secondary_code_offset>65536u-16u ||
       in.usc_offset>65536u-152u ||
       overlaps(in.fragment_offset,64,in.secondary_data_offset,16) ||
       overlaps(in.fragment_offset,64,in.secondary_code_offset,16) ||
       overlaps(in.secondary_data_offset,16,in.secondary_code_offset,16))return 1;
    /* Native FUN_13c5f8/13cd1c: address, ceil(4/4) temps and DOUTU.END. */
    image.pds_fragment[0]=in.usc_offset;image.pds_fragment[1]=8;
    image.pds_fragment[8]=0xf4000002u;
    image.raw00=0x200u;
    image.raw38=(uint64_t)in.fragment_offset|((uint64_t)in.secondary_code_offset<<32);
    image.raw40=(uint64_t)in.secondary_data_offset<<32;
    image.raw48=in.secondary_data_offset;
    image.raw50=(UINT64_C(1)<<55)|(UINT64_C(1)<<16)|1u;
    *out=image;return 0;
}
