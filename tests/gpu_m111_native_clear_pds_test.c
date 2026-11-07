/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "native_clear_pds.h"
static uint32_t u32(const unsigned char *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int main(void)
{
    unsigned char data[38],old[38],eot[10];unsigned i,j;
    const uint32_t temps[]={0,1,4,5,252};
    const uint32_t high[]={0,8,8,16,504};
    for(j=0;j<5;j++) {
        memset(data,0xa5,sizeof(data));memset(eot,0xb6,sizeof(eot));
        assert(!a04_m111_patch_background_pds(data+1,36,UINT64_C(0xba00123400),temps[j]));
        assert(u32(data+1)==0x123400 && u32(data+5)==high[j]);
        assert(u32(data+33)==0xf4000002u && data[0]==0xa5 && data[37]==0xa5);
        for(i=9;i<33;i++)assert(data[i]==0xa5);
        assert(!a04_m111_build_eot_task_data(eot+1,8,UINT64_C(0xba00123400),temps[j]));
        assert(!memcmp(eot+1,data+1,8) && eot[0]==0xb6 && eot[9]==0xb6);
    }
    memcpy(old,data,sizeof(data));
    assert(a04_m111_patch_background_pds(data+1,35,UINT64_C(0xba00123400),0));
    assert(a04_m111_patch_background_pds(data+1,36,UINT64_C(0xba00123402),0));
    assert(a04_m111_patch_background_pds(data+1,36,UINT64_C(0xba40000000),0));
    assert(a04_m111_patch_background_pds(data+1,36,UINT64_C(0xb900000000),0));
    assert(a04_m111_patch_background_pds(data+1,36,UINT64_C(0xba00123400),253));
    assert(!memcmp(old,data,sizeof(data)));
    assert(a04_m111_build_eot_task_data(data+1,7,UINT64_C(0xba00123400),0));
    assert(!memcmp(old,data,sizeof(data)));
    return 0;
}
