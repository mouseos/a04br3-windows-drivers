/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "windows_render_cold.h"
#include "windows_start.h"
#include "A04BR3RogueRuntimeLayout.h"
static struct a04_vendor_render_cold *sim;
static struct a04_rgx_fw_runtime *run;
static unsigned current,complete;
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
void a04_gpu_dma_barrier(void){}
void KeStallExecutionProcessor(unsigned us)
{
    RGXFWIF_KCCB_CMD *cmd;RGXFWIF_CCCB_CTL *ctl;unsigned char *bank;uint32_t off;
    (void)us;if(!complete)return;
    cmd=&run->kcommands[run->kctl->ui32ReadOffset];bank=sim->buffer[0].cpu;
    assert(cmd->eCmdType==RGXFWIF_KCCB_CMD_KICK);
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
    unsigned char *bank=aligned_alloc(4096,65536),*core=calloc(1,0x1000);
    RGXFWIF_CCB_CTL kctl={0},fctl={0};RGXFWIF_KCCB_CMD *cmd=calloc(128,sizeof(*cmd));
    struct a04_vendor_render_cold r={0};struct a04_vendor_m1_11_memory mem={0};
    struct a04_rgx_fw_runtime runtime={0};struct a04_vendor_start start={0};
    assert(bank && core && cmd);
    assert(!a04_vendor_render_build(bank,65536,0x81000000,0x82000000,A04_VENDOR_RENDER_SCRATCH_VA,1,&r.layout));
    kctl.ui32WrapMask=127;kctl.ui32CmdSize=sizeof(*cmd);fctl.ui32WrapMask=31;fctl.ui32CmdSize=sizeof(RGXFWIF_FWCCB_CMD);
    runtime.kctl=&kctl;runtime.fctl=&fctl;runtime.kcommands=cmd;mem.runtime=&runtime;mem.hardware_owned=TRUE;
    start.memory=&mem;start.online=TRUE;start.core=core;r.firmware=&mem;r.buffer[0].cpu=bank;r.mapped=TRUE;
    sim=&r;run=&runtime;complete=1;
    for(current=0;current<4;current++) {
        assert(a04_vendor_render_context_command(&r,&start,current,current==2?0xffffffffu:0)==STATUS_SUCCESS);
        assert(r.client_write[current]==(current==2?88u:48u));
    }
    assert(r.sequence==1 && !r.poisoned && !start.command_poisoned);
    current=2;complete=0;
    assert(a04_vendor_render_context_command(&r,&start,2,0xffffffffu)==STATUS_IO_TIMEOUT);
    assert(r.poisoned && start.command_poisoned);
    free(cmd);free(core);free(bank);return 0;
}
