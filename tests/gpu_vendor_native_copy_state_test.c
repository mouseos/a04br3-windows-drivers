/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "native_copy_state.h"
#include "render_resources.h"
int main(void)
{
    struct a04_vendor_copy_state out,before;
    struct a04_vendor_copy_state_input in={A04_VENDOR_RENDER_SCRATCH_VA,
        A04_VENDOR_RENDER_SCRATCH_VA+4096,A04_VENDOR_RENDER_SCRATCH_VA+8192};
    uint64_t t0,t1;
    assert(!a04_vendor_copy_state_build(&out,&in));
    t0=(uint64_t)out.shared[4]|((uint64_t)out.shared[5]<<32);
    t1=(uint64_t)out.shared[6]|((uint64_t)out.shared[7]<<32);
    assert(out.shared[0]==0 && out.shared[1]==0x2400 && out.shared[2]==0 && out.shared[3]==0);
    assert(t0==((UINT64_C(12)<<27)|(UINT64_C(1)<<11)|(UINT64_C(2)<<8)|(UINT64_C(3)<<5)|4|(UINT64_C(31)<<34)|(UINT64_C(31)<<48)));
    assert(t1==((in.source_va<<14)|31|(UINT64_C(1)<<60)|(UINT64_C(1)<<59)) && out.shared[8]==255);
    assert(out.pds_data[0]==(uint32_t)in.shared_va && out.pds_data[1]==(uint32_t)(in.shared_va>>32) &&
        out.pds_data[2]==0xf0000009 && !out.pds_data[3]);
    assert(out.pds_code[0]==0xc1400002 && out.pds_code[1]==0xf4020000 && out.pds_code[2]==0xd3000000 && !out.pds_code[3]);
    before=out;
    in.destination_va=A04_VENDOR_RENDER_SCRATCH_VA+12288;
    assert(!a04_vendor_copy_state_build(&out,&in) && !memcmp(&out,&before,sizeof(out)));
    in.source_va=A04_VENDOR_RENDER_SCRATCH_VA+16384;
    assert(!a04_vendor_copy_state_build(&out,&in));
    t1=(uint64_t)out.shared[6]|((uint64_t)out.shared[7]<<32);
    assert(t1==((in.source_va<<14)|31|(UINT64_C(1)<<60)|(UINT64_C(1)<<59)));
    in.source_va=A04_VENDOR_RENDER_SCRATCH_VA;before=out;in.destination_va=in.source_va;
    assert(a04_vendor_copy_state_build(&out,&in) && !memcmp(&out,&before,sizeof(out)));
    in.destination_va=A04_VENDOR_RENDER_SCRATCH_VA+4096;in.shared_va=in.destination_va+16;
    assert(a04_vendor_copy_state_build(&out,&in) && !memcmp(&out,&before,sizeof(out)));
    in.shared_va=A04_VENDOR_RENDER_SCRATCH_VA+131056;
    assert(a04_vendor_copy_state_build(&out,&in) && !memcmp(&out,&before,sizeof(out)));
    in.shared_va=A04_VENDOR_RENDER_SCRATCH_VA+8192;in.source_va++;
    assert(a04_vendor_copy_state_build(&out,&in) && !memcmp(&out,&before,sizeof(out)));
    {
        struct a04_vendor_copy_kick_input k={192,256,272,128};
        _Alignas(8) struct a04_vendor_copy_kick_image image,old;
        assert(!a04_vendor_copy_kick_build(&image,&k));
        assert(image.pds_fragment[0]==128 && image.pds_fragment[1]==8 &&
               image.pds_fragment[8]==0xf4000002u && image.raw00==0x200u);
        assert(image.raw38==((uint64_t)272<<32|192u) && image.raw40==((uint64_t)256<<32) &&
               image.raw48==256 && image.raw50==((UINT64_C(1)<<55)|(UINT64_C(1)<<16)|1) && image.raw58==0);
        old=image;k.secondary_code_offset=256;
        assert(a04_vendor_copy_kick_build(&image,&k) && !memcmp(&image,&old,sizeof(image)));
        k.secondary_code_offset=272;k.usc_offset=65536-148;
        assert(a04_vendor_copy_kick_build(&image,&k) && !memcmp(&image,&old,sizeof(image)));
        k.usc_offset=128;k.fragment_offset=65504;
        assert(a04_vendor_copy_kick_build(&image,&k) && !memcmp(&image,&old,sizeof(image)));
    }
    {
        uint32_t source[1024],destination[1024],shared[1024];unsigned i;
        struct a04_vendor_copy_check_result result,old;
        struct a04_vendor_copy_state_input fixed={A04_VENDOR_RENDER_SCRATCH_VA,
            A04_VENDOR_RENDER_SCRATCH_VA+4096,A04_VENDOR_RENDER_SCRATCH_VA+8192};
        assert(!a04_vendor_copy_state_build(&out,&fixed));
        for(i=0;i<1024;i++) {
            source[i]=a04_vendor_copy_pattern(i);destination[i]=source[i];
            shared[i]=i<9?out.shared[i]:0xa5a5a5a5u;
        }
        assert(source[0]!=source[256] && source[0]!=source[1023]);
        assert(!a04_vendor_copy_check(&result,source,destination,shared,1024,&out));
        assert(!result.source_mismatches && !result.destination_mismatches && !result.shared_mismatches &&
            result.source_first==1024 && result.destination_first==1024 && result.shared_first==1024);
        source[5]^=1;destination[5]^=1;destination[1023]^=1;shared[8]^=1;shared[1023]^=1;
        assert(!a04_vendor_copy_check(&result,source,destination,shared,1024,&out));
        assert(result.source_mismatches==1 && result.source_first==5 &&
            result.destination_mismatches==2 && result.destination_first==5 &&
            result.shared_mismatches==2 && result.shared_first==8);
        old=result;
        assert(a04_vendor_copy_check(&result,source,destination,shared,1023,&out) && !memcmp(&result,&old,sizeof(result)));
        assert(a04_vendor_copy_check(&result,source,source,shared,1024,&out) && !memcmp(&result,&old,sizeof(result)));
        assert(a04_vendor_copy_check(&result,source,destination,NULL,1024,&out) && !memcmp(&result,&old,sizeof(result)));
    }
    return 0;
}
