/* SPDX-License-Identifier: MIT. Model IRQL is not evidence of a real ISR. */
#define main old_native_backing_test_main
#include "gpu_vendor_windows_native_backing_test.c"
#undef main
#include "windows_native_job_private.h"
#include "windows_native_submit.h"
#include "windows_start.h"
#include "A04BR3RogueRuntimeLayout.h"
void KeStallExecutionProcessor(unsigned us){(void)us;assert(!"submit may not poll or stall");}
int main(void)
{
    struct fixture x;struct a04_vendor_native_cold *n=NULL;struct a04_vendor_start s={0};
    struct a04_rgx_fw_runtime f={0};RGXFWIF_CCB_CTL k={127,127,127,sizeof(RGXFWIF_KCCB_CMD)};
    RGXFWIF_CCB_CTL q={0,0,31,sizeof(RGXFWIF_FWCCB_CMD)};RGXFWIF_KCCB_CMD commands[128];
    uint32_t sync[2];UCHAR core[0xc00]={0};unsigned a,p;
    init(&x);assert(!a04_vendor_render_build(x.r.buffer[0].cpu,65536,
        (uint64_t)x.r.buffer[0].logical.QuadPart,(uint64_t)x.r.buffer[1].logical.QuadPart,
        UINT64_C(0x65c0000000),1,&x.r.layout));
    assert(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,64,800,600)==STATUS_SUCCESS);
    f.kctl=&k;f.fctl=&q;f.kcommands=commands;x.fw.runtime=&f;x.fw.hardware_owned=TRUE;
    s.memory=&x.fw;s.core=core;s.online=TRUE;a=allocs;p=pool_calls;
    memset(commands,0,sizeof(commands));host_irql=5;fail_at=allocs+1;fail_pool=pool_calls+1;
    assert(a04_vendor_native_submit_prepared_pair(n,&s,127)==STATUS_INVALID_DEVICE_STATE);
    n->candidate->attempted=TRUE;s.command_busy=1;
    assert(a04_vendor_native_submit_prepared_pair(n,&s,126)==STATUS_DEVICE_BUSY && k.ui32WriteOffset==127);
    q.ui32WriteOffset=1;
    assert(a04_vendor_native_submit_prepared_pair(n,&s,127)==STATUS_DEVICE_BUSY && k.ui32WriteOffset==127);
    q.ui32WriteOffset=0;
    assert(a04_vendor_native_submit_prepared_pair(n,&s,127)==STATUS_SUCCESS);
    assert(k.ui32WriteOffset==1 && commands[127].eDM==2 && commands[0].eDM==3);
    assert(allocs==a && pool_calls==p && !frees && !n->candidate->verified && !n->candidate->completed_frames);
    memcpy(sync,(UCHAR *)x.r.buffer[0].cpu+0x4c00,sizeof(sync));assert(!sync[0] && !sync[1]);
    assert(a04_vendor_native_submit_prepared_pair(n,&s,1)==STATUS_INVALID_DEVICE_STATE);
    puts("Native prepared submit: model IRQL5, zero allocate/wait/stall, empty queue and wrap, accepted != completed PASS");
    /* Published backing intentionally retained. No real GPU/device opened. */
    return 0;
}
