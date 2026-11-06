/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "native_ccb.h"
#include "native_record_fields.h"
int main(void)
{
    _Alignas(8) uint8_t ring[4096],before[4096],ta[A04_M111_TA_BYTES],frag[A04_M111_3D_BYTES];
    RGXFWIF_UFO ufo={{0xc0080200},1};RGXFWIF_CCB_CMD_HEADER h;
    RGXFWIF_KCCB_CMD kick;struct a04_m111_ccb_result r,old;
    /* Synthetic byte payloads test transport only; these are not GPU fixtures. */
    memset(ta,0x71,sizeof(ta));memset(frag,0x93,sizeof(frag));memset(ring,0x5a,sizeof(ring));
    assert(!a04_m111_ccb_append(ring,4096,0,0,A04_M111_CLIENT_TA,ta,sizeof(ta),7,8,&r));
    assert(r.write_offset==176 && r.header_offset==0 && !r.padding_bytes);
    memcpy(&h,ring,sizeof(h));assert(h.eCmdType==0x2abc80c9 && h.ui32CmdSize==136);
    assert(h.ui32ExtJobRef==7 && h.ui32IntJobRef==8 && !h.sWorkEstKickData.ui64Deadline);
    assert(!memcmp(ring+40,ta,sizeof(ta)));
    assert(!a04_m111_ccb_append(ring,4096,0,r.write_offset,A04_M111_CLIENT_UPDATE,&ufo,8,7,8,&r));
    assert(r.write_offset==224);
    memset(ring,0x5a,sizeof(ring));
    assert(!a04_m111_ccb_append(ring,4096,0,0,A04_M111_CLIENT_FENCE_PR,&ufo,8,7,8,&r));
    assert(r.write_offset==48);
    assert(!a04_m111_ccb_append(ring,4096,0,48,A04_M111_CLIENT_3D,frag,sizeof(frag),7,8,&r));
    assert(r.write_offset==424);
    assert(!a04_m111_ccb_append(ring,4096,0,424,A04_M111_CLIENT_UPDATE,&ufo,8,7,8,&r));
    assert(r.write_offset==472);
    assert(!a04_m111_ccb_append(ring,4096,304,4000,A04_M111_CLIENT_TA,ta,sizeof(ta),7,8,&r));
    assert(r.padding_bytes==96 && r.header_offset==0 && r.write_offset==176);
    memcpy(&h,ring+4000,sizeof(h));assert(h.eCmdType==0x2abc00dd && h.ui32CmdSize==56);
    memcpy(before,ring,4096);old=r;
    assert(a04_m111_ccb_append(ring,4096,176,4000,A04_M111_CLIENT_TA,ta,sizeof(ta),7,8,&r));
    assert(!memcmp(before,ring,4096) && !memcmp(&old,&r,sizeof(r)));
    assert(a04_m111_ccb_append(ring,4096,304,4080,A04_M111_CLIENT_TA,ta,sizeof(ta),7,8,&r));
    assert(a04_m111_ccb_append(ring,4096,0,0,A04_M111_CLIENT_TA,frag,sizeof(frag),7,8,&r));
    assert(a04_m111_ccb_append(ring,4096,0,0,A04_M111_CLIENT_TA,ring,136,7,8,&r));
    assert(!a04_m111_build_render_kick(&kick,0xc0080000,2,224,0xc0090100));
    assert(kick.eDM==2 && kick.eCmdType==RGXFWIF_KCCB_CMD_KICK && kick.uCmdData.sCmdKickData.ui32CWoffUpdate==224);
    assert(kick.uCmdData.sCmdKickData.apsCleanupCtl[0].ui32Addr==0xc00901d0);
    assert(!a04_m111_build_render_kick(&kick,0xc00800a0,3,472,0xc0090100));
    assert(kick.eDM==3 && kick.uCmdData.sCmdKickData.apsCleanupCtl[0].ui32Addr==0xc00901d8);
    assert(a04_m111_build_render_kick(&kick,0xc00800a0,3,4096,0xc0090100));
    return 0;
}
