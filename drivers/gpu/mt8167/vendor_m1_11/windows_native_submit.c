/* SPDX-License-Identifier: MIT */
#include "windows_native_submit.h"
#include "windows_native_job_private.h"
#include "windows_start.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
#include "../windows_hw.h"
NTSTATUS a04_vendor_native_submit_prepared_pair(struct a04_vendor_native_cold *n,
    struct a04_vendor_start *s,uint32_t expected)
{
    struct a04_vendor_render_cold *r;struct a04_vendor_native_candidate *c;
    struct a04_rgx_fw_runtime *f;RGXFWIF_CCB_CTL k,q;uint32_t next,second;
    if(!n || !s || expected>127u || !(r=n->render) || r->native_owner!=n || !(c=n->candidate) ||
       !c->attempted || c->verified || c->published || n->quarantined || r->poisoned || r->cleanup_started ||
       !s->online || !s->core || s->command_busy!=1 || s->command_poisoned || s->shutdown_started ||
       !s->memory || s->memory!=r->firmware || !s->memory->hardware_owned || !s->memory->uncached ||
       !(f=s->memory->runtime) || !f->kctl || !f->fctl || !f->kcommands ||
       c->pair.ta_kick.eCmdType!=RGXFWIF_KCCB_CMD_KICK || c->pair.ta_kick.eDM!=2 ||
       c->pair.three_d_kick.eCmdType!=RGXFWIF_KCCB_CMD_KICK || c->pair.three_d_kick.eDM!=3 ||
       c->pair.ta_kick.uCmdData.sCmdKickData.ui32CWoffUpdate!=c->pair.ta_write ||
       c->pair.three_d_kick.uCmdData.sCmdKickData.ui32CWoffUpdate!=c->pair.three_d_write)
        return STATUS_INVALID_DEVICE_STATE;
    a04_gpu_dma_barrier();
    if(a04_rgx_device_read_words(&k,f->kctl,sizeof(k)) ||
       a04_rgx_device_read_words(&q,f->fctl,sizeof(q)) || k.ui32WrapMask!=127u ||
       k.ui32CmdSize!=sizeof(RGXFWIF_KCCB_CMD) || k.ui32ReadOffset!=expected || k.ui32WriteOffset!=expected ||
       q.ui32WrapMask!=31u || q.ui32CmdSize!=sizeof(RGXFWIF_FWCCB_CMD) ||
       q.ui32ReadOffset>31u || q.ui32ReadOffset!=q.ui32WriteOffset)return STATUS_DEVICE_BUSY;
    next=(expected+1u)&127u;second=(next+1u)&127u;
    c->published=TRUE; /* Claim before any ambiguous Device-memory write. */
    if(a04_rgx_device_write_words(&f->kcommands[expected],&c->pair.ta_kick,sizeof(c->pair.ta_kick)) ||
       a04_rgx_device_write_words(&f->kcommands[next],&c->pair.three_d_kick,sizeof(c->pair.three_d_kick)))
        return STATUS_IO_DEVICE_ERROR;
    r->client_write[0]=c->pair.ta_write;r->client_write[1]=c->pair.three_d_write;
    /* Preserve the already-validated native two-kick ordering. */
    a04_gpu_dma_barrier();((volatile RGXFWIF_CCB_CTL *)f->kctl)->ui32WriteOffset=next;
    a04_gpu_dma_barrier();WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+0xb00),0x10);
    a04_gpu_dma_barrier();((volatile RGXFWIF_CCB_CTL *)f->kctl)->ui32WriteOffset=second;
    a04_gpu_dma_barrier();WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+0xb00),0x10);
    return STATUS_SUCCESS;
}
