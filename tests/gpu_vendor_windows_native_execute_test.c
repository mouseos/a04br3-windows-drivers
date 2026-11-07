/* SPDX-License-Identifier: MIT
 * Simulated FW consumes only this test's own RAM. No driver/device execution. */
#define main old_native_backing_test_main
#include "gpu_vendor_windows_native_backing_test.c"
#undef main
#include "windows_native_job_private.h"
#include "windows_start.h"
#include "A04BR3RogueRuntimeLayout.h"
static struct fixture *active;
static struct a04_vendor_native_cold *child_owner;
static struct a04_rgx_fw_runtime runtime;
static unsigned mode,commands;
void KeStallExecutionProcessor(unsigned us)
{
    RGXFWIF_CCB_CTL *k=runtime.kctl;RGXFWIF_KCCB_CMD *cmd;
    RGXFWIF_CCCB_CTL *ctl;RGXFWIF_CLEANUP_CTL *cleanup;unsigned dm,i;
    assert(us==50);
    if(mode==1 || (mode==4 && commands>=4))return; /* FW timeout, no execution. */
    while(k->ui32ReadOffset!=k->ui32WriteOffset){
        cmd=&runtime.kcommands[k->ui32ReadOffset];commands++;
        if(cmd->eCmdType==RGXFWIF_KCCB_CMD_KICK){
            assert(cmd->eDM==2 || cmd->eDM==3);dm=cmd->eDM==2?0:1;
            ctl=(void *)((UCHAR *)active->r.buffer[0].cpu+active->r.layout.control[dm]);
            ctl->ui32ReadOffset=ctl->ui32WriteOffset=cmd->uCmdData.sCmdKickData.ui32CWoffUpdate;
            ((uint32_t *)((UCHAR *)active->r.buffer[0].cpu+0x4c00))[dm]++;
            cleanup=(void *)((UCHAR *)active->r.buffer[0].cpu+
                cmd->uCmdData.sCmdKickData.apsCleanupCtl[0].ui32Addr-0xc0080000u);
            cleanup->ui32SubmittedCommands++;if(!(mode==3 && dm))cleanup->ui32ExecutedCommands++;
            if(dm)for(i=0;i<480000;i++){
#if A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
                /* Independent raster oracle for the lifetime model only;
                 * no shader/packet execution is claimed by this host test. */
                double x=((double)(i%800)+0.5)/800.0,y=((double)(i/800)+0.5)/600.0;
                uint32_t value=x+y<1.0?(child_owner->candidate->completed_frames?0xffcc9933u:0xff6633ccu):0xff000000u;
                if(mode==2)value^=1u;
#else
                uint32_t value=mode==2?0xff332210u:0xff332211u;
#endif
                *(uint32_t *)a04_vendor_native_storage_pointer(child_owner,1,i*4u,4u)=value;
            }
        }else if(cmd->eCmdType==RGXFWIF_KCCB_CMD_SLCFLUSHINVAL){assert(commands%4==3);}
        else {assert(cmd->eCmdType==RGXFWIF_KCCB_CMD_SYNC && commands%4==0);
            *(uint32_t *)runtime.command_sync=cmd->uCmdData.sSyncData.uiUpdateVal;
            if(mode==5 && commands==4){
                RGXFWIF_CLEANUP_CTL *bad=(void *)((UCHAR *)active->r.buffer[0].cpu+child_owner->addresses.ta_cleanup[0]-0xc0080000u);
                bad->ui32ExecutedCommands=0;
            }}
        k->ui32ReadOffset=(k->ui32ReadOffset+1)&127;
    }
}
static void run(unsigned fault,unsigned frames)
{
    struct fixture x;struct a04_vendor_native_cold *n=NULL;struct a04_vendor_start start={0};
    struct a04_vendor_native_result result;RGXFWIF_CCB_CTL k={0,0,127,sizeof(RGXFWIF_KCCB_CMD)};
    RGXFWIF_CCB_CTL q={0,0,31,sizeof(RGXFWIF_FWCCB_CMD)};RGXFWIF_KCCB_CMD queue[128];
    uint32_t sync=0;UCHAR core[0xc00]={0};NTSTATUS status;
    init(&x);assert(!a04_vendor_render_build(x.r.buffer[0].cpu,65536,
        (uint64_t)x.r.buffer[0].logical.QuadPart,(uint64_t)x.r.buffer[1].logical.QuadPart,
        UINT64_C(0x65c0000000),1,&x.r.layout));
    assert(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,1100,800,600)==STATUS_SUCCESS);
    memset(&runtime,0,sizeof(runtime));memset(queue,0,sizeof(queue));
    runtime.kctl=&k;runtime.fctl=&q;runtime.kcommands=queue;
    runtime.command_sync=&sync;runtime.command_sync_addr.ui32Addr=0xc0010000u;
    x.fw.runtime=&runtime;x.fw.hardware_owned=TRUE;start.memory=&x.fw;start.core=core;start.online=TRUE;
    active=&x;child_owner=n;mode=fault;commands=0;
    status=a04_vendor_native_run_clear_frames(n,&start,frames,&result);
    if(!fault){
        assert(status==STATUS_SUCCESS && result.phase==4 && result.matching_pixels==480000);
        assert(!result.mismatched_pixels && !result.guard_errors && commands%4==0 && n->candidate->verified);
        assert(!start.command_busy && !start.command_poisoned && !n->quarantined);
        assert(a04_vendor_native_run_once(n,&start,&result)==STATUS_INVALID_DEVICE_STATE);
        assert(a04_vendor_native_release_cold(&n)==STATUS_DEVICE_BUSY && !frees);
        x.fw.hardware_owned=FALSE;assert(a04_vendor_native_release_cold(&n)==STATUS_SUCCESS && !n);finish(&x);
    }else{
        assert(!NT_SUCCESS(status) && start.command_poisoned && x.r.poisoned && n->quarantined);
        if(fault>=4)assert(result.completed_frames==1);
        assert(a04_vendor_native_release_cold(&n)==STATUS_DEVICE_BUSY && !frees && !start.command_busy);
        _exit(0); /* Deliberately retained DMA in this isolated test process. */
    }
}
int main(void)
{
    unsigned fault;
#if A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
    run(0,2);
#else
    run(0,1);run(0,16);
#endif
    for(fault=1;fault<=5;fault++){
        pid_t p=fork();int status;assert(p>=0);if(!p)run(fault,
#if A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
        2
#else
        fault>=4?3:1
#endif
        );
        assert(waitpid(p,&status,0)==p && WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
#if A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
    puts("Triangle/texture source candidate: staged resource shadows, bounded 2-frame lifetime, poison/guard/fence model PASS; no GPU executed");
#else
    puts("Native publisher host model: 1/16 frames, ring wrap, late timeout, corrupt cleanup, pixel mismatch, one-shot and DMA retention PASS; no GPU executed");
#endif
    return 0;
}
