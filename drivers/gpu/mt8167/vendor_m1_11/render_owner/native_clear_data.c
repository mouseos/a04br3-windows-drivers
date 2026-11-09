/* SPDX-License-Identifier: MIT
 * VDM/PPP: board GLES OutputTerminateState 0x1c1fac.
 * Table bytes: original-driver capture04, ev_23a2611a79b90fdc1374766579b6795f0c473575fdd08b59e53b719d77707fc6.
 * Scissor CBUF type8: DoKickTA 0x1612a0 / 0x164ed4;
 * type-name table0x282028, ev_a48495a209e847b8dbae0a06a5fc91848076c409ae33577f897db9b04897724f.
 * Scissor lane layout: Mesa rogue/ipf.xml SCISSOR_WORD_0/1.
 * No captured GPU VA, task word, or resource blob is embedded here.
 */
#include "native_clear_data.h"
#include "native_terminate_stream.h"
static void le32(uint8_t *p,uint32_t v)
{ unsigned i;for(i=0;i<4;i++)p[i]=(uint8_t)(v>>(8u*i)); }
int a04_m111_build_clear_data(struct a04_m111_clear_data *out,
    uint64_t gpu,uint32_t width,uint32_t height)
{
    struct a04_m111_clear_data made={{0},0,0,0,0};
    struct a04_m111_terminate_stream stream;unsigned i;
    /* Two closed single-sample profiles. Scissor and terminate fields are
     * rebuilt from explicit dimensions; the HWRT producer has exact profiles
     * for both 600 and 1280. No shader/packet restrictions are removed. */
    if(!out || width!=800u || (height!=600u && height!=1280u) ||
       a04_m111_build_clear_terminate_stream(&stream,gpu,width,height))return 1;
    for(i=0;i<sizeof(stream.data);i++)made.bytes[i]=stream.data[i];
    le32(made.bytes+A04_M111_CLEAR_SCISSOR_OFFSET,width);
    le32(made.bytes+A04_M111_CLEAR_SCISSOR_OFFSET+4u,height);
    le32(made.bytes+A04_M111_CLEAR_SCISSOR_OFFSET+8u,width);
    le32(made.bytes+A04_M111_CLEAR_SCISSOR_OFFSET+12u,height);
    /* Neutral constant/slope/clamp float bits; capture contains zero here.
     * Bytes 156..159 are host alignment padding, not another bias field. */
    made.vdm_gpu=gpu;made.ppp_gpu=gpu+A04_M111_TERMINATE_PPP_OFFSET;
    made.scissor_gpu=gpu+A04_M111_CLEAR_SCISSOR_OFFSET;
    made.dbias_gpu=gpu+A04_M111_CLEAR_DBIAS_OFFSET;
    *out=made;return 0;
}
