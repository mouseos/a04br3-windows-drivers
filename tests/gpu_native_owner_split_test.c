/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Real core + modeled OS/DMA/FW RAM. Model IRQL is not a real ISR.
 * No device, no synthetic Dxgk completion notification, no GPU execution. */
#define main old_native_backing_test_main
#include "gpu_vendor_windows_native_backing_test.c"
#undef main
#include "windows_native_job_private.h"
#include "native_owner_mdl.h"
#include "windows_start.h"
#include "A04BR3RogueRuntimeLayout.h"
static unsigned holds,sg_gets,sg_puts,cpu_flushes,commands,fault;
static struct fixture *active;
static struct a04_vendor_native_cold *owner;
static struct a04_rgx_fw_runtime fw;
static NTSTATUS retain(void *ctx,PMDL m){assert(ctx==m && !holds);holds++;return STATUS_SUCCESS;}
static void drop(void *ctx,PMDL m){assert(ctx==m && holds);holds--;}
void *MmGetSystemAddressForMdlSafe(PMDL m,int p){(void)p;return m->va;}
void KeInitializeEvent(KEVENT *e,int t,BOOLEAN b){(void)t;e->signaled=b;}
void KeSetEvent(KEVENT *e,int n,BOOLEAN w){(void)n;(void)w;e->signaled=1;}
NTSTATUS KeWaitForSingleObject(void *e,int a,int b,BOOLEAN c,void *d)
{(void)a;(void)b;(void)c;(void)d;assert(((KEVENT *)e)->signaled);return STATUS_SUCCESS;}
void KeFlushIoBuffers(PMDL m,BOOLEAN read,BOOLEAN dma){assert(m && dma);if(read)cpu_flushes++;}
static NTSTATUS get_sg(PDMA_ADAPTER a,PDEVICE_OBJECT p,PMDL m,void *va,ULONG bytes,PDRIVER_LIST_CONTROL cb,void *ctx,BOOLEAN w)
{
    ULONG at=(ULONG)((UCHAR *)va-(UCHAR *)m->va);
    PSCATTER_GATHER_LIST sg=calloc(1,sizeof(*sg));(void)a;
    assert(!w && bytes<=65536 && sg);sg_gets++;
    sg->NumberOfElements=1;sg->Elements[0].Address.QuadPart=(uint64_t)m->pfns[at/4096]*4096;
    sg->Elements[0].Length=bytes;cb(p,NULL,sg,ctx);return STATUS_SUCCESS;
}
static void put_sg(PDMA_ADAPTER a,PSCATTER_GATHER_LIST sg,BOOLEAN w)
{(void)a;assert(!w);sg_puts++;free(sg);}
void KeStallExecutionProcessor(unsigned us)
{
    RGXFWIF_CCB_CTL *k=fw.kctl;
    assert(host_irql==PASSIVE_LEVEL && us==50); /* submit cannot wait */
    if(fault==1)return;
    while(k->ui32ReadOffset!=k->ui32WriteOffset){
        RGXFWIF_KCCB_CMD *cmd=&fw.kcommands[k->ui32ReadOffset];commands++;
        if(cmd->eCmdType==RGXFWIF_KCCB_CMD_KICK){
            unsigned dm=cmd->eDM==2?0:1;assert(cmd->eDM==2 || cmd->eDM==3);
            RGXFWIF_CCCB_CTL *ctl=(void *)((UCHAR *)active->r.buffer[0].cpu+active->r.layout.control[dm]);
            RGXFWIF_CLEANUP_CTL *cl=(void *)((UCHAR *)active->r.buffer[0].cpu+cmd->uCmdData.sCmdKickData.apsCleanupCtl[0].ui32Addr-0xc0080000u);
            ctl->ui32ReadOffset=ctl->ui32WriteOffset=cmd->uCmdData.sCmdKickData.ui32CWoffUpdate;
            ((uint32_t *)((UCHAR *)active->r.buffer[0].cpu+0x4c00))[dm]++;
            cl->ui32SubmittedCommands++;cl->ui32ExecutedCommands++;
            if(dm)for(unsigned i=0;i<480000;i++)
                *(uint32_t *)a04_vendor_native_storage_pointer(owner,1,i*4u,4u)=
                    owner->candidate->clear_pixel^((fault==2 && !i)?1u:0u);
        }else if(cmd->eCmdType==RGXFWIF_KCCB_CMD_SYNC)
            *(uint32_t *)fw.command_sync=cmd->uCmdData.sSyncData.uiUpdateVal;
        else assert(cmd->eCmdType==RGXFWIF_KCCB_CMD_SLCFLUSHINVAL);
        k->ui32ReadOffset=(k->ui32ReadOffset+1)&127;
    }
}
static void check(unsigned mode)
{
    struct fixture x;struct a04_vendor_native_cold *n=NULL;
    struct a04_native_mdl_binding *binding=NULL;struct fake_mdl mdl={0};
    PFN_NUMBER pfns[469];struct a04_native_mdl_lease lease={&mdl,retain,drop};
    struct a04_vendor_start s={0};struct a04_vendor_native_result result;
    RGXFWIF_CCB_CTL k={127,127,127,sizeof(RGXFWIF_KCCB_CMD)},q={0,0,31,sizeof(RGXFWIF_FWCCB_CMD)};
    RGXFWIF_KCCB_CMD queue[128];uint32_t sync=0;UCHAR core[0xc00]={0};
    init(&x);holds=sg_gets=sg_puts=cpu_flushes=commands=0;fault=mode;
    x.ops.GetScatterGatherList=get_sg;x.ops.PutScatterGatherList=put_sg;
    assert(!a04_vendor_render_build(x.r.buffer[0].cpu,65536,(uint64_t)x.r.buffer[0].logical.QuadPart,
        (uint64_t)x.r.buffer[1].logical.QuadPart,UINT64_C(0x65c0000000),1,&x.r.layout));
    assert(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,64,800,600)==STATUS_SUCCESS);
    mdl.va=aligned_alloc(4096,469*4096);assert(mdl.va);mdl.bytes=469*4096;
    mdl.MdlFlags=MDL_PAGES_LOCKED;mdl.pfns=pfns;
    for(unsigned i=0;i<469;i++)pfns[i]=0x3000+i+(i/16)*0x100;
    assert(a04_native_mdl_bind(&binding,&x.adapter,&x,&mdl,mdl.bytes,&lease)==STATUS_SUCCESS);
    assert(a04_vendor_native_attach_mdl_target_at_cold(n,binding,500u*4096u,0x7fa3b4c5u)==STATUS_SUCCESS);
    assert(a04_vendor_native_target_before_fw_start(n)==STATUS_SUCCESS);
    memset(&fw,0,sizeof(fw));memset(queue,0,sizeof(queue));
    fw.kctl=&k;fw.fctl=&q;fw.kcommands=queue;fw.command_sync=&sync;fw.command_sync_addr.ui32Addr=0xc0010000u;
    x.fw.runtime=&fw;x.fw.hardware_owned=TRUE;s.memory=&x.fw;s.core=core;s.online=TRUE;s.attempted=TRUE;
    active=&x;owner=n;unsigned a=allocs,p=pool_calls,flushes=cpu_flushes;
    assert(a04_vendor_native_owner_submit(n,&s,127)==STATUS_INVALID_DEVICE_STATE); /* wrong IRQL */
    host_irql=5;fail_at=allocs+1;fail_pool=pool_calls+1;
    assert(a04_vendor_native_owner_submit(n,&s,126)==STATUS_DEVICE_BUSY && !s.command_busy && !n->candidate->attempted);
    assert(a04_vendor_native_owner_submit(n,&s,127)==STATUS_SUCCESS);
    assert(allocs==a && pool_calls==p && !commands && s.command_busy==1 &&
           n->candidate->owner_pending && !n->candidate->verified && !n->candidate->completed_frames);
    assert(k.ui32WriteOffset==1 && queue[127].eDM==2 && queue[0].eDM==3);
    assert(a04_vendor_native_owner_submit(n,&s,1)==STATUS_INVALID_DEVICE_STATE);
    assert(a04_vendor_native_owner_finish_passive(n,&s,&result)==STATUS_INVALID_DEVICE_STATE);
    host_irql=PASSIVE_LEVEL;
    NTSTATUS status=a04_vendor_native_owner_finish_passive(n,&s,&result);
    assert(!s.command_busy && allocs==a && pool_calls==p && *(uint32_t *)(core+0x8ac)==0); /* no IRQ enable/notification */
    if(!mode){
        assert(status==STATUS_SUCCESS && result.phase==4 && result.matching_pixels==480000 && !result.mismatched_pixels &&
            !result.guard_errors && result.completed_frames==1 && commands==4 && cpu_flushes==flushes+1);
        assert(!n->candidate->owner_pending && n->candidate->verified && !n->quarantined);
        assert(a04_vendor_native_owner_finish_passive(n,&s,&result)==STATUS_INVALID_DEVICE_STATE);
    }else{
        assert(!NT_SUCCESS(status) && n->quarantined && s.command_poisoned && x.r.poisoned);
        assert(cpu_flushes==flushes+(mode==2));
    }
    assert(a04_native_mdl_release_cold(&binding)==STATUS_DEVICE_BUSY && binding && !sg_puts && holds);
    assert(a04_vendor_native_release_cold(&n)==STATUS_DEVICE_BUSY && n); /* no fabricated quiescence */
}
int main(void)
{
    for(unsigned mode=0;mode<3;mode++){
        pid_t pid=fork();int status;assert(pid>=0);if(!pid){check(mode);_exit(0);}
        assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    }
    puts("same-MDL owner split: nonwaiting accept, wrap, PASSIVE actual fence/cache/readback, timeout/mismatch poison, retained DMA; no IRQ notification/GPU execution");
}
