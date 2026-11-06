/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "windows_render_cold.h"
#include "windows_copy_scratch.h"
#include "windows_start.h"
#include "A04BR3RogueRuntimeLayout.h"
static struct a04_vendor_render_cold *sim;
static struct a04_rgx_fw_runtime *run;
static struct a04_vendor_copy_scratch *copy_sim;
static unsigned current,complete,cleanups,inject_cleanup;
void a04_test_interlock_hook(volatile LONG *p,LONG value,LONG expected)
{
    (void)p; (void)value; (void)expected;
    if(inject_cleanup) { sim->cleanup_started=TRUE; inject_cleanup=0; }
}
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
void a04_gpu_dma_barrier(void){}
void KeStallExecutionProcessor(unsigned us)
{
    RGXFWIF_KCCB_CMD *cmd;RGXFWIF_CCCB_CTL *ctl;unsigned char *bank;uint32_t off;
    (void)us;if(!complete)return;
    cmd=&run->kcommands[run->kctl->ui32ReadOffset];bank=sim->buffer[0].cpu;
    if(cmd->eCmdType==RGXFWIF_KCCB_CMD_CLEANUP) {
        assert(cmd->uCmdData.sCleanupData.eCleanupType==RGXFWIF_CLEANUP_FWCOMMONCONTEXT);
        assert(cmd->eDM==(cleanups==0?2u:(cleanups==2?0u:3u)));
        *(uint32_t *)run->command_sync=(complete==3 || (complete==4 && cleanups==1))?3u:1u;
        run->kctl->ui32ReadOffset=run->kctl->ui32WriteOffset;cleanups++;return;
    }
    if(cmd->eCmdType==RGXFWIF_KCCB_CMD_SLCFLUSHINVAL) {
        if(complete==2)return;
        run->kctl->ui32ReadOffset=run->kctl->ui32WriteOffset;return;
    }
    if(cmd->eCmdType==RGXFWIF_KCCB_CMD_SYNC) {
        *(uint32_t *)run->command_sync=cmd->uCmdData.sSyncData.uiUpdateVal;
        run->kctl->ui32ReadOffset=run->kctl->ui32WriteOffset;return;
    }
    assert(cmd->eCmdType==RGXFWIF_KCCB_CMD_KICK);
    if(current==5) {
        ctl=(void *)(bank+sim->layout.control[3]);
        off=cmd->uCmdData.sCmdKickData.ui32CWoffUpdate;
        assert(off>=296 && !memcmp(bank+sim->layout.commands[3]+off-296+40,copy_sim->frozen_task,208));
        ctl->ui32ReadOffset=ctl->ui32WriteOffset=off;
        memcpy(copy_sim->destination.cpu,copy_sim->source.cpu,4096);
        *(uint32_t *)(void *)(bank+sim->layout.sync)=sim->sequence;
        run->kctl->ui32ReadOffset=run->kctl->ui32WriteOffset;return;
    }
    if(current==4) {
        ctl=(void *)(bank+sim->layout.control[3]);
        ctl->ui32ReadOffset=ctl->ui32WriteOffset=cmd->uCmdData.sCmdKickData.ui32CWoffUpdate;
        for(unsigned row=0;row<32;row++) for(unsigned col=0;col<32;col++)
            *(uint32_t *)((unsigned char *)(sim->screen_surface ? sim->screen_surface->pixels.cpu : sim->buffer[4].cpu)+sim->clear_first+row*sim->clear_row_bytes+col*4)=sim->clear.expected;
        *(uint32_t *)(void *)(bank+sim->layout.sync)=sim->sequence;
        run->kctl->ui32ReadOffset=run->kctl->ui32WriteOffset;return;
    }
    ctl=(void *)(bank+sim->layout.control[current]);
    off=cmd->uCmdData.sCmdKickData.ui32CWoffUpdate;
    ctl->ui32ReadOffset=ctl->ui32WriteOffset=off;
    if(current==2) *(uint32_t *)(void *)(bank+sim->layout.sync)=sim->sequence;
    else {
        RGXFWIF_FWCOMMONCONTEXT *ctx;
        if(current==0)ctx=&((RGXFWIF_FWRENDERCONTEXT *)(void *)(bank+sim->layout.context))->sTAContext;
        else if(current==1)ctx=&((RGXFWIF_FWRENDERCONTEXT *)(void *)(bank+sim->layout.context))->s3DContext;
        else ctx=(void *)(bank+sim->layout.tq);
        ctx->ui32Priority=0;
    }
    run->kctl->ui32ReadOffset=run->kctl->ui32WriteOffset;
}
int main(void)
{
    unsigned char *bank=aligned_alloc(4096,65536),*core=calloc(1,0x8000),*target=aligned_alloc(4096,65536);
    uint32_t pds[16]={0};uint32_t command_sync=0;struct a04_vendor_clear_result clear;
    RGXFWIF_CCB_CTL kctl={0},fctl={0};RGXFWIF_KCCB_CMD *cmd=calloc(128,sizeof(*cmd));
    struct a04_vendor_render_cold r={0};struct a04_vendor_m1_11_memory mem={0};
    struct a04_rgx_fw_runtime runtime={0};struct a04_vendor_start start={0};
    assert(bank && core && cmd && target);memset(target,0xa5,65536);
    assert(!a04_vendor_render_build(bank,65536,0x81000000,0x82000000,A04_VENDOR_RENDER_SCRATCH_VA,1,&r.layout));
    kctl.ui32WrapMask=127;kctl.ui32CmdSize=sizeof(*cmd);fctl.ui32WrapMask=31;fctl.ui32CmdSize=sizeof(RGXFWIF_FWCCB_CMD);
    runtime.kctl=&kctl;runtime.fctl=&fctl;runtime.kcommands=cmd;runtime.command_sync=&command_sync;runtime.command_sync_addr.ui32Addr=0xc0040040;mem.runtime=&runtime;mem.hardware_owned=TRUE;
    start.memory=&mem;start.online=TRUE;start.core=core;r.firmware=&mem;r.buffer[0].cpu=bank;r.mapped=TRUE;
    r.buffer[4].cpu=target;r.buffer[4].bytes=65536;r.buffer[7].cpu=pds;r.clear_ready=TRUE;r.clear_first=(64u*128u+64u)*4u;r.clear_row_bytes=512u;
    assert(!a04_vendor_clear_image_build(&r.clear,A04_VENDOR_RENDER_SCRATCH_VA,0));
    sim=&r;run=&runtime;complete=1;
    {
      RGXFWIF_KCCB_CMD before=cmd[kctl.ui32WriteOffset];uint32_t oldwrite=kctl.ui32WriteOffset;
      r.copy_owner=&mem;
      assert(a04_vendor_render_clear_once(&r,&start,&clear)==STATUS_DEVICE_BUSY);
      assert(!start.command_busy && !r.clear_attempted && !r.client_write[3] &&
          kctl.ui32WriteOffset==oldwrite && !memcmp(&before,&cmd[oldwrite],sizeof(before)));
      r.copy_owner=NULL;
    }
    /* Simulate cleanup winning after the caller's initial check, immediately
     * before it acquires command_busy. Neither path may publish a command. */
    { RGXFWIF_KCCB_CMD before; uint32_t oldwrite=kctl.ui32WriteOffset;
      memcpy(&before,&cmd[oldwrite],sizeof(before));
      inject_cleanup=1;
      assert(a04_vendor_render_context_command(&r,&start,0,0)==STATUS_INVALID_DEVICE_STATE);
      assert(!inject_cleanup && !start.command_busy && kctl.ui32WriteOffset==oldwrite &&
             !r.client_write[0] && !memcmp(&before,&cmd[oldwrite],sizeof(before)));
      r.cleanup_started=FALSE;inject_cleanup=1;
      assert(a04_vendor_render_clear_once(&r,&start,&clear)==STATUS_INVALID_DEVICE_STATE);
      assert(!inject_cleanup && !start.command_busy && kctl.ui32WriteOffset==oldwrite &&
             !r.client_write[3] && !r.clear_attempted && !memcmp(&before,&cmd[oldwrite],sizeof(before)));
      r.cleanup_started=FALSE;
    }
    for(current=0;current<4;current++) {
        assert(a04_vendor_render_context_command(&r,&start,current,current==2?0xffffffffu:0)==STATUS_SUCCESS);
        assert(r.client_write[current]==(current==2?88u:48u));
    }
    assert(r.sequence==1 && !r.poisoned && !start.command_poisoned);
    current=4;assert(a04_vendor_render_clear_once(&r,&start,&clear)==STATUS_SUCCESS);
    assert(clear.zeros==1024 && !clear.guard_errors && clear.phase==4 && r.clear_attempted);
    assert(clear.completed_task==r.sequence && clear.completed_flush==start.command_sequence && clear.completed_task && clear.completed_flush);
    { uint32_t before=pds[0],pixel=((uint32_t*)target)[0];
      r.copy_owner=&mem;
      assert(a04_vendor_render_next_tile(&r,&start,1,&clear)==STATUS_INVALID_DEVICE_STATE);
      assert(!start.command_busy && pds[0]==before && ((uint32_t*)target)[0]==pixel && r.diagnostic_tile_index==0);
      r.copy_owner=NULL;
    }
    assert(a04_vendor_render_next_tile(&r,&start,1,&clear)==STATUS_SUCCESS);
    assert(clear.zeros==1024 && !clear.guard_errors && clear.phase==4);
    assert(a04_vendor_render_next_tile(&r,&start,2,&clear)==STATUS_SUCCESS);
    assert(clear.zeros==1024 && !clear.guard_errors && clear.first==0x78563412u);
    assert(a04_vendor_render_next_tile(&r,&start,2,&clear)==STATUS_INVALID_DEVICE_STATE);
    assert(a04_vendor_render_clear_once(&r,&start,&clear)==STATUS_INVALID_PARAMETER);
    { unsigned failed=99;complete=3;
      assert(a04_vendor_render_cleanup_contexts(&r,&start,&failed)==STATUS_DEVICE_BUSY && failed==0 && !r.cleanup_mask && r.cleanup_started && !r.poisoned);
      assert(a04_vendor_render_context_command(&r,&start,2,0xffffffffu)==STATUS_INVALID_PARAMETER);
      complete=4;cleanups=0;assert(a04_vendor_render_cleanup_contexts(&r,&start,&failed)==STATUS_DEVICE_BUSY && failed==1 && r.cleanup_mask==1u && cleanups==2);
      complete=1;cleanups=1;assert(a04_vendor_render_cleanup_contexts(&r,&start,&failed)==STATUS_SUCCESS && failed==4 && r.cleanup_mask==15u && cleanups==4);
      assert(a04_vendor_render_context_command(&r,&start,2,0xffffffffu)==STATUS_INVALID_PARAMETER);
      assert(a04_vendor_render_clear_once(&r,&start,&clear)==STATUS_INVALID_PARAMETER);
      r.cleanup_mask=0;r.cleanup_started=FALSE;
      complete=0;assert(a04_vendor_render_cleanup_contexts(&r,&start,&failed)==STATUS_IO_TIMEOUT && failed==0 && r.poisoned && start.command_poisoned);
      assert(a04_vendor_render_cleanup_contexts(&r,&start,&failed)==STATUS_INVALID_PARAMETER);
      r.poisoned=FALSE;r.cleanup_started=FALSE;start.command_poisoned=FALSE;kctl.ui32ReadOffset=kctl.ui32WriteOffset;
    }
    current=2;complete=0;
    assert(a04_vendor_render_context_command(&r,&start,2,0xffffffffu)==STATUS_IO_TIMEOUT);
    assert(r.poisoned && start.command_poisoned);
    memset(target,0xa5,65536);
    memset(&r,0,sizeof(r));memset(&kctl,0,sizeof(kctl));memset(&fctl,0,sizeof(fctl));memset(cmd,0,128*sizeof(*cmd));
    assert(!a04_vendor_render_build(bank,65536,0x81000000,0x82000000,A04_VENDOR_RENDER_SCRATCH_VA,1,&r.layout));
    kctl.ui32WrapMask=127;kctl.ui32CmdSize=sizeof(*cmd);fctl.ui32WrapMask=31;fctl.ui32CmdSize=sizeof(RGXFWIF_FWCCB_CMD);
    r.firmware=&mem;r.buffer[0].cpu=bank;r.buffer[4].cpu=target;r.buffer[4].bytes=65536;r.mapped=TRUE;r.clear_ready=TRUE;r.clear_first=(64u*128u+64u)*4u;r.clear_row_bytes=512u;
    assert(!a04_vendor_clear_image_build(&r.clear,A04_VENDOR_RENDER_SCRATCH_VA,0));
    start.command_poisoned=FALSE;start.command_sequence=0;current=4;complete=2;
    assert(a04_vendor_render_clear_once(&r,&start,&clear)==STATUS_IO_TIMEOUT);
    assert(clear.phase==3 && clear.kread!=clear.kwrite && clear.expected==1 && r.poisoned && start.command_poisoned);
    assert(!clear.completed_task && !clear.completed_flush);
    assert(clear.provisional && clear.zeros==1024 && !clear.guard_errors);
    {
        struct a04_vendor_surface screen={0};
        unsigned char *pixels=aligned_alloc(4096,4259840u);assert(pixels);memset(pixels,0xa5,4259840u);
        memset(&r,0,sizeof(r));memset(&kctl,0,sizeof(kctl));memset(&fctl,0,sizeof(fctl));memset(cmd,0,128*sizeof(*cmd));
        assert(!a04_vendor_render_build(bank,65536,0x81000000,0x82000000,A04_VENDOR_RENDER_SCRATCH_VA,1,&r.layout));
        kctl.ui32WrapMask=127;kctl.ui32CmdSize=sizeof(*cmd);fctl.ui32WrapMask=31;fctl.ui32CmdSize=sizeof(RGXFWIF_FWCCB_CMD);
        screen.pixels.cpu=pixels;screen.pixels.bytes=4259840u;screen.bytes=4259840u;screen.prepared=TRUE;screen.hardware_exposed=TRUE;
        r.firmware=&mem;r.buffer[0].cpu=bank;r.buffer[4].cpu=target;r.buffer[4].bytes=65536;r.mapped=TRUE;
        r.clear_ready=TRUE;r.screen_surface=&screen;r.clear_first=1248u*3328u+768u*4u;r.clear_row_bytes=3328u;
        assert(!a04_vendor_clear_image_build(&r.clear,A04_VENDOR_RENDER_SCRATCH_VA,0));
        start.command_poisoned=FALSE;start.command_sequence=0;current=4;complete=1;
        assert(a04_vendor_render_clear_once(&r,&start,&clear)==STATUS_SUCCESS);
        assert(clear.zeros==1024 && !clear.guard_errors && clear.phase==4);
        { uint32_t seq=r.sequence, fence=start.command_sequence, pds_before[16];
          memcpy(pds_before,pds,sizeof(pds));
          assert(a04_vendor_render_next_tile(&r,&start,1,&clear)==STATUS_INVALID_DEVICE_STATE);
          inject_cleanup=1;
          assert(a04_vendor_render_repeat_screen(&r,&start,1,&clear)==STATUS_INVALID_DEVICE_STATE);
          assert(!inject_cleanup && !start.command_busy && r.sequence==seq && r.clear_verified);
          r.cleanup_started=FALSE;
          assert(a04_vendor_render_repeat_screen(&r,&start,1,&clear)==STATUS_SUCCESS);
          assert(clear.zeros==1024 && !clear.guard_errors && clear.phase==4 && r.sequence==seq+1 && start.command_sequence==fence+1);
          assert(!memcmp(pds_before,pds,sizeof(pds)));
          assert(a04_vendor_render_repeat_screen(&r,&start,1,&clear)==STATUS_INVALID_DEVICE_STATE);
          assert(a04_vendor_render_repeat_screen(&r,&start,2,&clear)==STATUS_SUCCESS);
          assert(clear.zeros==1024 && !clear.guard_errors && clear.phase==4 && r.sequence==seq+2 && start.command_sequence==fence+2);
          assert(!memcmp(pds_before,pds,sizeof(pds)));
        }
        pixels[0]=0; r.clear_attempted=FALSE;
        assert(a04_vendor_render_clear_once(&r,&start,&clear)==STATUS_DATA_ERROR);
        assert(clear.guard_errors==1 && !r.clear_verified && !clear.completed_task && !clear.completed_flush);
        memset(pixels,0xa5,4259840u);r.clear_attempted=FALSE;r.diagnostic_tile_index=0;complete=1;
        assert(a04_vendor_render_clear_once(&r,&start,&clear)==STATUS_SUCCESS);
        complete=2;
        assert(a04_vendor_render_repeat_screen(&r,&start,1,&clear)==STATUS_IO_TIMEOUT);
        assert(clear.phase==3 && r.poisoned && start.command_poisoned && !r.clear_verified);
        assert(a04_vendor_render_repeat_screen(&r,&start,2,&clear)==STATUS_INVALID_DEVICE_STATE);
        free(pixels);
    }
    {
        struct a04_vendor_copy_scratch copy={0};uint32_t src[1024],dst[1024],shared[1024];unsigned i;
        memset(target,0xa5,65536);
        memset(&r,0,sizeof(r));memset(&kctl,0,sizeof(kctl));memset(&fctl,0,sizeof(fctl));memset(cmd,0,128*sizeof(*cmd));
        assert(!a04_vendor_render_build(bank,65536,0x81000000,0x82000000,A04_VENDOR_RENDER_SCRATCH_VA,1,&r.layout));
        kctl.ui32WrapMask=127;kctl.ui32CmdSize=sizeof(*cmd);fctl.ui32WrapMask=31;fctl.ui32CmdSize=sizeof(RGXFWIF_FWCCB_CMD);
        r.firmware=&mem;r.buffer[0].cpu=bank;r.buffer[4].cpu=target;r.buffer[4].bytes=65536;r.mapped=TRUE;r.clear_ready=TRUE;
        copy.render=&r;copy.prepared=copy.exposed=copy.usc_ready=copy.task_frozen=TRUE;
        copy.source.cpu=src;copy.destination.cpu=dst;copy.shared.cpu=shared;
        copy.source.bytes=copy.destination.bytes=copy.shared.bytes=4096;r.copy_owner=&copy;
        for(i=0;i<1024;i++){src[i]=a04_vendor_copy_pattern(i);dst[i]=shared[i]=0xa5a5a5a5u;}
        for(i=0;i<9;i++)shared[i]=copy.state.shared[i];
        copy.frozen_task[0]=0x200;copy_sim=&copy;start.command_poisoned=FALSE;start.command_sequence=0;current=5;complete=1;
        assert(a04_vendor_render_copy_once(&copy,&start,&clear)==STATUS_SUCCESS);
        assert(clear.phase==4 && clear.zeros==1024 && !clear.guard_errors && r.clear_attempted && copy.exposed);
        assert(a04_vendor_render_copy_once(&copy,&start,&clear)==STATUS_INVALID_PARAMETER);
        r.clear_attempted=FALSE;r.clear_verified=FALSE;r.client_write[3]=0;r.sequence=0;
        memset(&kctl,0,sizeof(kctl));memset(&fctl,0,sizeof(fctl));memset(cmd,0,128*sizeof(*cmd));
        assert(!a04_vendor_render_build(bank,65536,0x81000000,0x82000000,A04_VENDOR_RENDER_SCRATCH_VA,1,&r.layout));
        kctl.ui32WrapMask=127;kctl.ui32CmdSize=sizeof(*cmd);fctl.ui32WrapMask=31;fctl.ui32CmdSize=sizeof(RGXFWIF_FWCCB_CMD);
        complete=0;
        assert(a04_vendor_render_copy_once(&copy,&start,&clear)==STATUS_IO_TIMEOUT);
        assert(clear.phase==1 && r.poisoned && start.command_poisoned && copy.exposed && r.copy_owner==&copy);
        assert(a04_vendor_render_copy_once(&copy,&start,&clear)==STATUS_INVALID_PARAMETER);
        r.poisoned=FALSE;r.clear_attempted=FALSE;r.client_write[3]=0;r.sequence=0;start.command_poisoned=FALSE;
        memset(&kctl,0,sizeof(kctl));memset(&fctl,0,sizeof(fctl));memset(cmd,0,128*sizeof(*cmd));
        assert(!a04_vendor_render_build(bank,65536,0x81000000,0x82000000,A04_VENDOR_RENDER_SCRATCH_VA,1,&r.layout));
        kctl.ui32WrapMask=127;kctl.ui32CmdSize=sizeof(*cmd);fctl.ui32WrapMask=31;fctl.ui32CmdSize=sizeof(RGXFWIF_FWCCB_CMD);
        complete=2;
        assert(a04_vendor_render_copy_once(&copy,&start,&clear)==STATUS_IO_TIMEOUT);
        assert(clear.phase==3 && clear.provisional && !clear.zeros && r.poisoned && start.command_poisoned && copy.exposed);
        /* Arbitrary caller data never becomes a GPU address. Output is only
         * returned after the copy and the separate flush/fence have completed. */
        { uint32_t in[1024],out[1024];unsigned n,j;
          r.poisoned=FALSE;r.clear_attempted=FALSE;r.clear_verified=FALSE;r.client_write[3]=0;r.sequence=0;
          start.command_poisoned=FALSE;start.command_sequence=0;
          memset(&kctl,0,sizeof(kctl));memset(&fctl,0,sizeof(fctl));memset(cmd,0,128*sizeof(*cmd));
          assert(!a04_vendor_render_build(bank,65536,0x81000000,0x82000000,A04_VENDOR_RENDER_SCRATCH_VA,1,&r.layout));
          kctl.ui32WrapMask=127;kctl.ui32CmdSize=sizeof(*cmd);fctl.ui32WrapMask=31;fctl.ui32CmdSize=sizeof(RGXFWIF_FWCCB_CMD);
          complete=1;
          for(n=0;n<3;n++) {
            for(j=0;j<1024;j++){in[j]=j*31337u+n;out[j]=0;}
            assert(a04_vendor_render_copy_buffer(&copy,&start,in,out,sizeof(in))==STATUS_SUCCESS);
            assert(!memcmp(in,out,sizeof(in)) && !r.poisoned && !copy.service_expected_active);
          }
          { uint32_t old=r.client_write[3],sequence=r.sequence,write=kctl.ui32WriteOffset;
            r.client_write[3]=3800u;memset(out,0x69,sizeof(out));
            assert(a04_vendor_render_copy_buffer(&copy,&start,in,out,sizeof(in))==STATUS_DEVICE_BUSY);
            assert(r.sequence==sequence && kctl.ui32WriteOffset==write && !r.poisoned);
            for(j=0;j<1024;j++)assert(out[j]==0x69696969u);
            r.client_write[3]=old;
          }
          memset(out,0x69,sizeof(out));complete=0;
          assert(a04_vendor_render_copy_buffer(&copy,&start,in,out,sizeof(in))==STATUS_IO_TIMEOUT);
          assert(r.poisoned && start.command_poisoned && copy.exposed && copy.service_expected_active);
          for(j=0;j<1024;j++)assert(out[j]==0x69696969u);
          assert(a04_vendor_render_copy_buffer(&copy,&start,in,out,sizeof(in))==STATUS_INVALID_DEVICE_STATE);
        }
    }
    free(cmd);free(core);free(bank);free(target);return 0;
}
