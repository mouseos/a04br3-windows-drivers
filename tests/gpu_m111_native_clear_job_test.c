/* SPDX-License-Identifier: MIT
 * CPU integration of record/code/data producer with real native CCB builders.
 * No GPU execution. Capture projection regression lives in the evidence logs.
 */
#include <assert.h>
#include <string.h>
#include "native_clear_job.h"
#include "native_pair.h"
static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint64_t u64(const uint8_t *p)
{ return u32(p)|((uint64_t)u32(p+4)<<32); }
int main(void)
{
    struct a04_m111_clear_job j,before;
    struct a04_m111_clear_job_input in={UINT64_C(0x8000000000),UINT64_C(0x65c0358000),
        UINT64_C(0xba00010000),UINT64_C(0xa800010000),UINT64_C(0x65c0340000),
        UINT64_C(0x65c0340080),0xc00848c0,0xc0084c00,{17,34,51,255}};
    uint8_t ta_ring[4096]={0},three_ring[4096]={0},workspace[8192];
    struct a04_m111_shadow_ring ta={ta_ring,0,0},three={three_ring,0,0};
    struct a04_m111_pair_input pair={0xc0080000,0xc00800a0,0xc00848c0,
        0xc0084c00,0xc0084c04,0,1,1,1};
    struct a04_m111_pair_result result;
    assert(!a04_m111_build_clear_job_800x600_cold(&j,&in));
    /* Walk the producer's references, rather than only checking constants. */
    assert(u64(j.ta+0x10)==j.data.vdm_gpu && u32(j.ta+0x40)==0x100c0);
    assert((((uint64_t)(u32(j.data.bytes)&255u)<<32)|u32(j.data.bytes+4))==j.data.ppp_gpu);
    assert(u32(j.data.bytes+8)==0xc0000000u);
    assert(u32(j.data.bytes+64)==0x02000000 && u32(j.data.bytes+68)==0x00c40025);
    assert(u64(j.three+0x38)==j.data.scissor_gpu && u64(j.three+0x40)==j.data.dbias_gpu);
    assert(u32(j.data.bytes+128)==800 && u32(j.data.bytes+132)==600);
    assert(u32(j.three+4)==0xff332211u && u32(j.code.pds+0x40)==0xff332211u);
    assert(u32(j.three+0x28)==0x202);
    assert(u32(j.three+0x30)==0x100a0 && u32(j.three+0x34)==0x10080);
    assert(u64(j.code.pds+0xc0)==in.usc_page_gpu+0x20-UINT64_C(0xba00000000));
    /* Follow PBE output relocation into EOT data, not record CSR addresses. */
    assert((u64(j.code.pds+0x88)&UINT64_C(0x3fffffffff))==in.output_gpu/4);
    assert(!a04_m111_prepare_shadow_pair(&ta,&three,&pair,j.ta,j.three,
        workspace,sizeof(workspace),&result));
    assert(result.ta_write==224 && result.three_d_write==472);
    assert(!memcmp(ta_ring+40,j.ta,sizeof(j.ta)));
    assert(!memcmp(three_ring+88,j.three,sizeof(j.three)));
    assert(result.ta_kick.uCmdData.sCmdKickData.apsCleanupCtl[0].ui32Addr==in.hwrt_fw+0xd0);
    assert(result.three_d_kick.uCmdData.sCmdKickData.apsCleanupCtl[0].ui32Addr==in.hwrt_fw+0xd8);
    /* Move ALL owned code/data pages: no capture VA or old task survives. */
    in.data_page_gpu+=0x2000;in.usc_page_gpu+=0x2000;in.pds_page_gpu+=0x2000;
    in.output_gpu+=0x200000;in.rgba[0]=255;
    assert(!a04_m111_build_clear_job_800x600_cold(&j,&in));
    assert(u64(j.ta+0x10)==in.data_page_gpu && u32(j.ta+0x40)==0x120c0);
    assert(u64(j.code.pds+0xc0)==0x12020);
    assert(u32(j.three+4)==0xff3322ffu && u32(j.code.pds+0x40)==0xff3322ffu);
    before=j;
    in.data_page_gpu=in.dummy_region_gpu;
    assert(a04_m111_build_clear_job_800x600_cold(&j,&in));
    in.data_page_gpu=UINT64_C(0x65c0358001);
    assert(a04_m111_build_clear_job_800x600_cold(&j,&in));
    in.data_page_gpu=UINT64_C(0x65c0358000);in.pr_fence_fw=in.hwrt_fw;
    assert(a04_m111_build_clear_job_800x600_cold(&j,&in));
    assert(!memcmp(&j,&before,sizeof(j)));
    assert(a04_m111_build_clear_job_800x600_cold(&j,0));
    assert(a04_m111_build_clear_job_800x600_cold(0,&in));
    return 0;
}
