/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "native_hwrt_profile.h"
static float number(uint32_t bits)
{ float value;memcpy(&value,&bits,4);return value; }
static uint32_t bits(float value)
{ uint32_t word;memcpy(&word,&value,4);return word; }
int main(void)
{
    struct a04_m111_target_storage target;
    struct a04_m111_mlist_storage ml;
    struct a04_m111_hwrt_input hwrt[2],old[2];
    RGXFWIF_HWRTDATA bank;const uint32_t fl[2]={0xc0084840,0xc0084880};
    unsigned i,h;
    assert(!a04_m111_layout_pm_mlists(0x100000,UINT64_C(0x65c0231000),8192,&ml));
    for(h=600;h<=1280;h+=680) {
        assert(!a04_m111_layout_target_storage(800,h,UINT64_C(0x65c0200000),0xa000,
            UINT64_C(0x65c020a000),0x21000,&target));
        assert(!a04_m111_build_single_rt_hwrt_profile(hwrt,800,h,&target,&ml));
        for(i=0;i<2;i++) {
            const struct a04_m111_hwrt_input *p=hwrt+i;
            assert(p->merge_lower_x==bits(number(0x3e89309c)/800.0f));
            assert(p->merge_lower_y==bits(number(0x3e89309c)/(float)h));
            assert(p->merge_upper_x==bits(number(0x3fddb3d9)/800.0f));
            assert(p->merge_upper_y==bits(number(0x3fddb3d9)/(float)h));
            assert(p->merge_scale_x==bits(12800.0f/number(0x3fbb67b2)));
            assert(p->merge_scale_y==bits(((float)h*16.0f)/number(0x3fbb67b2)));
            assert(!a04_m111_build_hwrt(&bank,p,0xc0084800,fl));
            assert(bank.ui32PPPScreen==((h-1u)<<16|799u));
            assert(bank.ui32TEScreen==(((h+15u)/16u-1u)<<12|49u));
            assert(bank.uiRgnHeaderSize==(h==600?1900u:4000u));
            assert(bank.ui32TPCSize==(h==600?0x8000u:0x20000u));
            assert(bank.ui32MTileStride==bank.uiRgnHeaderSize);
            assert(bank.sMacrotileArrayDevVAddr.uiAddr==0 && !bank.sRTCDevVAddr.uiAddr);
            assert(!bank.ui32TPCStride && !bank.ui32TEAA);
            assert(bank.psPMMListDevVAddr.uiAddr==ml.gpu[i]);
        }
    }
    memcpy(old,hwrt,sizeof(old));target.tpc_bytes++;
    assert(a04_m111_build_single_rt_hwrt_profile(hwrt,800,1280,&target,&ml));
    assert(!memcmp(old,hwrt,sizeof(old)));target.tpc_bytes--;
    assert(a04_m111_build_single_rt_hwrt_profile(hwrt,800,601,&target,&ml));
    assert(!memcmp(old,hwrt,sizeof(old)));
    return 0;
}
