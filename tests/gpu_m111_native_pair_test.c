/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "native_pair.h"
#include "native_record_fields.h"
static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int main(void)
{
    uint8_t g[4096],f[4096],before_g[4096],before_f[4096],scratch[8192];
    uint8_t ta[A04_M111_TA_BYTES],frag[A04_M111_3D_BYTES];
    struct a04_m111_shadow_ring gr={g,0,0},fr={f,0,0};
    struct a04_m111_pair_input p={0xc0080000,0xc00800a0,0xc0090100,0xc0087000,0xc0087004,5,17,9,10};
    struct a04_m111_ta_fields gt={0};struct a04_m111_3d_fields ft={0};
    struct a04_m111_pair_result result,old;
    /* Synthetic transport fixture, never a rendering packet/golden. */
    memset(g,0xa5,sizeof(g));memset(f,0x5a,sizeof(f));
    memset(ta,0x31,sizeof(ta));memset(frag,0x42,sizeof(frag));
    gt.hwrt=p.hwrt;gt.pr_fence_addr=p.ta_sync;gt.pr_fence_value=p.ta_previous_value;
    ft.hwrt=p.hwrt;
    assert(!a04_m111_patch_ta_fields(ta,sizeof(ta),&gt));
    assert(!a04_m111_patch_3d_fields(frag,sizeof(frag),&ft));
    assert(!a04_m111_prepare_shadow_pair(&gr,&fr,&p,ta,frag,scratch,sizeof(scratch),&result));
    assert(result.ta_write==224 && result.three_d_write==472);
    assert(u32(g)==A04_M111_CLIENT_TA && u32(g+4)==136 && u32(g+8)==9 && u32(g+12)==10);
    assert(!memcmp(g+40,ta,sizeof(ta)) && u32(g+176)==A04_M111_CLIENT_UPDATE);
    assert(u32(g+216)==p.ta_sync && u32(g+220)==6);
    assert(u32(f)==A04_M111_CLIENT_FENCE_PR && u32(f+40)==p.ta_sync && u32(f+44)==6);
    assert(u32(f+48)==A04_M111_CLIENT_3D && !memcmp(f+88,frag,sizeof(frag)));
    assert(u32(f+424)==A04_M111_CLIENT_UPDATE && u32(f+464)==p.three_d_sync && u32(f+468)==17);
    assert(result.ta_kick.eDM==2 && result.three_d_kick.eDM==3);
    assert(result.ta_kick.uCmdData.sCmdKickData.ui32CWoffUpdate==224);
    assert(result.three_d_kick.uCmdData.sCmdKickData.ui32CWoffUpdate==472);
    assert(result.ta_kick.uCmdData.sCmdKickData.apsCleanupCtl[0].ui32Addr==p.hwrt+0xd0);
    assert(result.three_d_kick.uCmdData.sCmdKickData.apsCleanupCtl[0].ui32Addr==p.hwrt+0xd8);
    /* Whole groups wrap before their first command, not after a PR fence. */
    gr.read=2000;gr.write=4000;fr.read=2000;fr.write=3800;
    assert(!a04_m111_prepare_shadow_pair(&gr,&fr,&p,ta,frag,scratch,sizeof(scratch),&result));
    assert(u32(g+4000)==0x2abc00dd && u32(g+4004)==56);
    assert(u32(f+3800)==0x2abc00dd && u32(f+3804)==256);
    assert(u32(f)==A04_M111_CLIENT_FENCE_PR && u32(f+48)==A04_M111_CLIENT_3D);
    assert(result.ta_write==224 && result.three_d_write==472 && gr.write==4000 && fr.write==3800);
    memcpy(before_g,g,4096);memcpy(before_f,f,4096);old=result;
    fr.read=472; /* 3D ring lacks one byte; TA must not be committed alone. */
    assert(a04_m111_prepare_shadow_pair(&gr,&fr,&p,ta,frag,scratch,sizeof(scratch),&result));
    assert(!memcmp(g,before_g,4096) && !memcmp(f,before_f,4096) && !memcmp(&result,&old,sizeof(old)));
    fr.read=2000;
    assert(a04_m111_prepare_shadow_pair(&gr,&fr,&p,ta,frag,g,8192,&result));
    ta[0x58]^=1;
    assert(a04_m111_prepare_shadow_pair(&gr,&fr,&p,ta,frag,scratch,sizeof(scratch),&result));ta[0x58]^=1;
    ta[0x70]=1;
    assert(a04_m111_prepare_shadow_pair(&gr,&fr,&p,ta,frag,scratch,sizeof(scratch),&result));ta[0x70]=0;
    p.three_d_sync=p.hwrt;
    assert(a04_m111_prepare_shadow_pair(&gr,&fr,&p,ta,frag,scratch,sizeof(scratch),&result));p.three_d_sync=0xc0087004;
    p.three_d_context=p.ta_context+8;
    assert(a04_m111_prepare_shadow_pair(&gr,&fr,&p,ta,frag,scratch,sizeof(scratch),&result));p.three_d_context=0xc00800a0;
    p.ta_previous_value=UINT32_MAX;
    assert(a04_m111_prepare_shadow_pair(&gr,&fr,&p,ta,frag,scratch,sizeof(scratch),&result));
    assert(!memcmp(g,before_g,4096) && !memcmp(f,before_f,4096));
    return 0;
}
