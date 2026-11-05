/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded native KCCB SYNC transport. Algorithm derived from the MIT option
 * of Imagination m1.11ED5425693 rgxfwutils.c RGXSendCommandRaw/RGXWaitForFWOp,
 * rgx_common.h DM constants and rgx_cr_defs_km.h MTS register definitions.
 * ABI declarations and original copyright/license are in A04BR3RogueDDKABI.h.
 * No user command bytes, context addresses, shaders or arbitrary UFOs accepted.
 */
#ifndef A04BR3_ROGUE_COMMAND_H
#define A04BR3_ROGUE_COMMAND_H
#include <stdbool.h>
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
enum { A04_RGX_DM_GP=0,A04_RGX_DM_TA=2,A04_RGX_DM_3D=3 };
enum a04_rgx_command_result {
    A04_CCB_OK=0,A04_CCB_INPUT=1,A04_CCB_CORRUPT=2,A04_CCB_FULL=3,
    A04_CCB_TIMEOUT=4,A04_CCB_BUSY=5,A04_CCB_OFFLINE=6,A04_CCB_FW_REQUEST=7,
    A04_CCB_POWER_DENIED=8
};
/* CPU view MUST be verified noncache or hardware coherent; Windows common buffer
 * CacheEnabled=FALSE alone does not prove either property.
 * Mixed CPU/FW fields in CCB_CTL share a cache line: cleaning a cached CPU
 * write offset would overwrite a concurrently updated FW read offset.
 * Callbacks publish/acquire must order memory vs device MTS, not only CPU.
 */
struct a04_rgx_command_io {
    void *context;
    void (*publish)(void *);
    void (*acquire)(void *);
    void (*kick)(void *,uint32_t,uint32_t);
    void (*wait_us)(void *,uint32_t);
};
struct a04_rgx_command_state {uint32_t sequence;bool poisoned;};
static inline void *a04_rgx_command_memory(const struct a04_rgx_fw_runtime *r,
                                         uint32_t addr,uint32_t bytes)
{
    const uint32_t base=0xc0000000U+A04_RGX_MAIN_OFFSET;
    if (!r || !r->main.cpu || r->main.bytes!=A04_RGX_MAIN_BYTES ||
        r->main.heap_offset!=A04_RGX_MAIN_OFFSET || r->main.used>r->main.bytes ||
        addr<base || (addr&3) || addr-base>r->main.used ||
        bytes>r->main.used-(addr-base)) return NULL;
    return r->main.cpu+(addr-base);
}
static inline int a04_rgx_command_fence(struct a04_rgx_fw_runtime *r,
    struct a04_rgx_command_state *state,const struct a04_rgx_command_io *io,uint32_t dm,
    uint32_t polls,uint32_t delay_us)
{
    if (!r || !state || !io || !io->publish || !io->acquire || !io->kick || !io->wait_us ||
        (dm!=A04_RGX_DM_GP && dm!=A04_RGX_DM_TA && dm!=A04_RGX_DM_3D) ||
        !polls || !delay_us || delay_us>1000 || polls>100000/delay_us ||
        !r->init || state->sequence==0xffffffffU) return A04_CCB_INPUT;
    if (state->poisoned) return A04_CCB_OFFLINE;
    RGXFWIF_CCB_CTL *ctl=(RGXFWIF_CCB_CTL *)a04_rgx_command_memory(r,r->init->psKernelCCBCtl.ui32Addr,16);
    RGXFWIF_KCCB_CMD *cmds=(RGXFWIF_KCCB_CMD *)a04_rgx_command_memory(r,r->init->psKernelCCB.ui32Addr,128*56);
    volatile uint32_t *sync=(volatile uint32_t *)a04_rgx_command_memory(r,r->command_sync_addr.ui32Addr,64);
    RGXFWIF_CCB_CTL *fctl=(RGXFWIF_CCB_CTL *)a04_rgx_command_memory(r,r->init->psFirmwareCCBCtl.ui32Addr,16);
    if (!ctl || ctl!=r->kctl || !cmds || cmds!=r->kcommands || !sync || sync!=r->command_sync || !fctl || fctl!=r->fctl)
        return A04_CCB_INPUT;
    io->acquire(io->context);
    uint32_t read=ctl->ui32ReadOffset,write=ctl->ui32WriteOffset;
    if (ctl->ui32WrapMask!=127 || ctl->ui32CmdSize!=56 || read>127 || write>127 ||
        fctl->ui32WrapMask!=31 || fctl->ui32CmdSize!=40 ||
        fctl->ui32ReadOffset>31 || fctl->ui32WriteOffset>31) {
        state->poisoned=true;return A04_CCB_CORRUPT;
    }
    /* FW requests require a typed handler; never silently discard them. */
    if (fctl->ui32ReadOffset!=fctl->ui32WriteOffset) return A04_CCB_FW_REQUEST;
    uint32_t next=(write+1)&127;
    if (next==read) return A04_CCB_FULL;
    /* This transport is the sole producer and serializes fence completion.
     * An unexpected outstanding command is never overwritten or replayed. */
    if (read!=write) {state->poisoned=true;return A04_CCB_CORRUPT;}
    uint32_t sequence=++state->sequence;
    *sync=0;
    RGXFWIF_KCCB_CMD cmd={0};
    cmd.eCmdType=RGXFWIF_KCCB_CMD_SYNC;cmd.eDM=dm;
    cmd.uCmdData.sSyncData.sSyncObjDevVAddr=r->command_sync_addr;
    cmd.uCmdData.sSyncData.uiUpdateVal=sequence;
    if (a04_rgx_device_write_words(&cmds[write],&cmd,sizeof(cmd))) {
        state->poisoned=true;return A04_CCB_CORRUPT;
    }
    io->publish(io->context); /* complete command + sync clear BEFORE write offset */
    ctl->ui32WriteOffset=next;
    (void)ctl->ui32WriteOffset;
    io->publish(io->context);
    /* Native DDK always schedules GP, command.eDM selects target data master. */
    io->kick(io->context,0x0b00,0x10);
    for (uint32_t i=0;i<polls;i++) {
        io->acquire(io->context);
        uint32_t value=*sync,done=ctl->ui32ReadOffset;
        if (done>127 || (value && value!=sequence) || ctl->ui32WriteOffset!=next ||
            ctl->ui32WrapMask!=127 || ctl->ui32CmdSize!=56) {
            state->poisoned=true;return A04_CCB_CORRUPT;
        }
        /* Queue consumption alone is not execution completion. */
        if (value==sequence && done==next) return A04_CCB_OK;
        io->wait_us(io->context,delay_us);
    }
    state->poisoned=true;return A04_CCB_TIMEOUT;
}

/* m1.11 rgxpower.c RGXPrePowerState: publish POW_OFF_REQ, then require both
 * queue consumption and the dedicated power sync value, plus OFF state.
 * The shared queue must be coherent or uncached, as for the fence above.
 * A firmware acknowledgement alone does NOT prove fabric drain or permit DMA
 * buffer reclamation; IRQ drain and RGXStop checks remain separate duties. */
enum a04_rgx_power_action { A04_POWER_NORMAL_OFF, A04_POWER_FORCE_IDLE, A04_POWER_FORCED_OFF };
static inline int a04_rgx_command_power_transition(struct a04_rgx_fw_runtime *r,
    struct a04_rgx_command_state *state,const struct a04_rgx_command_io *io,
    enum a04_rgx_power_action action,uint32_t polls,uint32_t delay_us)
{
    RGXFWIF_CCB_CTL *ctl,*fctl;RGXFWIF_KCCB_CMD *cmds,cmd={0};
    volatile uint32_t *sync;uint32_t read,write,next;
    if(!r || !state || !io || !io->publish || !io->acquire || !io->kick || !io->wait_us ||
       !r->init || !r->trace || !r->osconfig || (action!=A04_POWER_NORMAL_OFF && action!=A04_POWER_FORCE_IDLE && action!=A04_POWER_FORCED_OFF) || !polls || !delay_us ||
       delay_us>1000 || polls>100000/delay_us) return A04_CCB_INPUT;
    if(state->poisoned) return A04_CCB_OFFLINE;
    ctl=(RGXFWIF_CCB_CTL *)a04_rgx_command_memory(r,r->init->psKernelCCBCtl.ui32Addr,16);
    cmds=(RGXFWIF_KCCB_CMD *)a04_rgx_command_memory(r,r->init->psKernelCCB.ui32Addr,128*56);
    fctl=(RGXFWIF_CCB_CTL *)a04_rgx_command_memory(r,r->init->psFirmwareCCBCtl.ui32Addr,16);
    if(!ctl || ctl!=r->kctl || !cmds || cmds!=r->kcommands || !fctl || fctl!=r->fctl ||
       a04_rgx_command_memory(r,r->init->sOSConfig.ui32Addr,sizeof(*r->osconfig))!=r->osconfig ||
       a04_rgx_command_memory(r,r->init->sTraceBufCtl.ui32Addr,sizeof(*r->trace))!=r->trace) return A04_CCB_INPUT;
    sync=(volatile uint32_t *)a04_rgx_command_memory(r,r->osconfig->sPowerSync.ui32Addr,4);
    if(!sync || sync!=r->power_sync) return A04_CCB_INPUT;
    io->acquire(io->context);
    read=ctl->ui32ReadOffset;write=ctl->ui32WriteOffset;
    if(ctl->ui32WrapMask!=127 || ctl->ui32CmdSize!=56 || read>127 || write>127 ||
       fctl->ui32WrapMask!=31 || fctl->ui32CmdSize!=40 ||
       fctl->ui32ReadOffset>31 || fctl->ui32WriteOffset>31) {
        state->poisoned=true;return A04_CCB_CORRUPT;
    }
    if(fctl->ui32ReadOffset!=fctl->ui32WriteOffset) return A04_CCB_FW_REQUEST;
    next=(write+1)&127;
    if(next==read) return A04_CCB_FULL;
    if(read!=write) {state->poisoned=true;return A04_CCB_CORRUPT;}
    /* Reject the initial zero/OFF state: it cannot prove a transition. */
    if(r->trace->ePowState!=RGXFWIF_POW_ON && r->trace->ePowState!=RGXFWIF_POW_IDLE &&
       r->trace->ePowState!=RGXFWIF_POW_FORCED_IDLE) return A04_CCB_INPUT;
    if(action==A04_POWER_FORCED_OFF && r->trace->ePowState!=RGXFWIF_POW_FORCED_IDLE) return A04_CCB_INPUT;
    if(action==A04_POWER_FORCE_IDLE && r->trace->ePowState==RGXFWIF_POW_FORCED_IDLE) return A04_CCB_INPUT;
    *sync=0;cmd.eCmdType=RGXFWIF_KCCB_CMD_POW;cmd.eDM=A04_RGX_DM_GP;
    if(action==A04_POWER_FORCE_IDLE) {
        cmd.uCmdData.sPowData.ePowType=RGXFWIF_POW_FORCED_IDLE_REQ;
        cmd.uCmdData.sPowData.uPoweReqData.ePowRequestType=RGXFWIF_POWER_FORCE_IDLE;
    } else {
        cmd.uCmdData.sPowData.ePowType=RGXFWIF_POW_OFF_REQ;
        cmd.uCmdData.sPowData.uPoweReqData.bForced=action==A04_POWER_FORCED_OFF;
    }
    if (a04_rgx_device_write_words(&cmds[write],&cmd,sizeof(cmd))) {
        state->poisoned=true;return A04_CCB_CORRUPT;
    }
    io->publish(io->context);
    ctl->ui32WriteOffset=next;(void)ctl->ui32WriteOffset;io->publish(io->context);
    io->kick(io->context,0x0b00,0x10);
    for(uint32_t i=0;i<polls;i++) {
        uint32_t value,done;io->acquire(io->context);
        value=*sync;done=ctl->ui32ReadOffset;
        if(done>127 || value>1 || ctl->ui32WriteOffset!=next ||
           ctl->ui32WrapMask!=127 || ctl->ui32CmdSize!=56) {
            state->poisoned=true;return A04_CCB_CORRUPT;
        }
        if(value==1 && done==next)
            return r->trace->ePowState==(action==A04_POWER_FORCE_IDLE ? RGXFWIF_POW_FORCED_IDLE : RGXFWIF_POW_OFF) ? A04_CCB_OK : A04_CCB_POWER_DENIED;
        io->wait_us(io->context,delay_us);
    }
    state->poisoned=true;return A04_CCB_TIMEOUT;
}
static inline int a04_rgx_command_power_off(struct a04_rgx_fw_runtime *r,
    struct a04_rgx_command_state *state,const struct a04_rgx_command_io *io,
    uint32_t polls,uint32_t delay_us)
{ return a04_rgx_command_power_transition(r,state,io,A04_POWER_NORMAL_OFF,polls,delay_us); }
_Static_assert(sizeof(RGXFWIF_KCCB_CMD)==56 && sizeof(RGXFWIF_CCB_CTL)==16,"KCCB ABI");
_Static_assert((unsigned)RGXFWIF_KCCB_CMD_SYNC==0x2abc0068U,"DDK SYNC opcode");
#endif
