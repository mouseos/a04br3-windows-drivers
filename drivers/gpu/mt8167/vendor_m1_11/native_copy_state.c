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
    /* FUN_12d28c writes raw58 from the optional format backing. For native
     * 0x22, FUN_13ee34 selects bit5 of row 0x195f1c (0x08), hence zero. */
    image.raw58=0;
    *out=image;return 0;
}

uint32_t a04_vendor_copy_pattern(uint32_t index)
{ return 0x5a00c3a5u ^ (index * 0x9e3779b1u); }

int a04_vendor_copy_check(struct a04_vendor_copy_check_result *out,
    const uint32_t *source,const uint32_t *destination,const uint32_t *shared,
    size_t snapshot_words,const struct a04_vendor_copy_state *state)
{
    struct a04_vendor_copy_check_result result={0,0,0,
        A04_VENDOR_COPY_NO_MISMATCH,A04_VENDOR_COPY_NO_MISMATCH,A04_VENDOR_COPY_NO_MISMATCH};
    uintptr_t p[5];size_t n[5]={sizeof(*out),4096,4096,4096,sizeof(*state)};
    unsigned i,j;
    if(snapshot_words!=A04_VENDOR_COPY_WORDS)return 1;
    p[0]=(uintptr_t)out;p[1]=(uintptr_t)source;p[2]=(uintptr_t)destination;
    p[3]=(uintptr_t)shared;p[4]=(uintptr_t)state;
    for(i=0;i<5;i++) {
        if(!p[i] || (p[i]&3u) || p[i]>UINTPTR_MAX-n[i])return 1;
        for(j=0;j<i;j++)if(overlaps(p[i],n[i],p[j],n[j]))return 1;
    }
    for(i=0;i<A04_VENDOR_COPY_WORDS;i++) {
        uint32_t expected=a04_vendor_copy_pattern(i);
        uint32_t shared_expected=i<9 ? state->shared[i] : 0xa5a5a5a5u;
        if(source[i]!=expected) {
            if(!result.source_mismatches)result.source_first=i;
            result.source_mismatches++;
        }
        if(destination[i]!=expected) {
            if(!result.destination_mismatches)result.destination_first=i;
            result.destination_mismatches++;
        }
        if(shared[i]!=shared_expected) {
            if(!result.shared_mismatches)result.shared_first=i;
            result.shared_mismatches++;
        }
    }
    *out=result;return 0;
}

/* The native producer at 12c310 selects sample count one. 129f88 writes
 * pixel bounds divided by 16, and 12bfb4 packs extent Y / extent X into
 * raw18. Source image and PBE fields retain the proven native22 encoding. */
int a04_vendor_copy_wide_state_build(struct a04_vendor_wide_state *out,uint64_t source,
    uint32_t width,uint32_t height,uint32_t pitch)
{
    struct a04_vendor_wide_state image={0};uint64_t bytes,t0,t1,csr,clip;
    if(!out || ((uintptr_t)out&3u) || (uintptr_t)out>UINTPTR_MAX-sizeof(*out) ||
       !width || !height || width>16368u || height>16368u || pitch<width ||
       pitch>16384u || (pitch&31u) || source<4096u || (source&127u))return 1;
    bytes=(uint64_t)pitch*height*4u;
    if(source>=(UINT64_C(1)<<40) || bytes>(UINT64_C(1)<<40)-source)return 1;
    image.shared[1]=0x2400u;
    t0=(UINT64_C(12)<<27)|(UINT64_C(1)<<11)|(UINT64_C(2)<<8)|(UINT64_C(3)<<5)|4u|
        ((uint64_t)(width-1u)<<34)|((uint64_t)(height-1u)<<48);
    t1=(source<<14)|(pitch-1u)|(UINT64_C(1)<<60)|(UINT64_C(1)<<59);
    image.shared[4]=(uint32_t)t0;image.shared[5]=(uint32_t)(t0>>32);
    image.shared[6]=(uint32_t)t1;image.shared[7]=(uint32_t)(t1>>32);image.shared[8]=255u;
    image.tile_size=((width+15u)/16u)<<16|((height+15u)/16u);
    csr=(UINT64_C(2)<<56)|(((uint64_t)(pitch-1u)&UINT64_C(0x1fffe))<<33)|
        (UINT64_C(1)<<23)|(UINT64_C(2)<<26)|(UINT64_C(3)<<29);
    clip=((uint64_t)(height-1u)<<32)|(width-1u);
    image.csr[0]=(uint32_t)csr;image.csr[1]=(uint32_t)(csr>>32);
    image.csr[2]=(uint32_t)clip;image.csr[3]=(uint32_t)(clip>>32);
    *out=image;return 0;
}
