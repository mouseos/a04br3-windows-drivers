/* SPDX-License-Identifier: MIT
 * Bounded private candidate publisher. NativeClearJob alone remains cold;
 * only the separately default-OFF activation flag adds the D0 caller.
 * No IOCTL or service admits arbitrary GPU packet bytes.
 */
#include "windows_native_job_private.h"
#include "windows_start.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
#include "../windows_hw.h"
static int read_queue(struct a04_rgx_fw_runtime *f,RGXFWIF_CCB_CTL *k)
{
    RGXFWIF_CCB_CTL q;
    a04_gpu_dma_barrier();
    return a04_rgx_device_read_words(k,f->kctl,sizeof(*k)) ||
        a04_rgx_device_read_words(&q,f->fctl,sizeof(q)) || k->ui32WrapMask!=127u ||
        k->ui32CmdSize!=sizeof(RGXFWIF_KCCB_CMD) || k->ui32ReadOffset>127u || k->ui32WriteOffset>127u ||
        q.ui32WrapMask!=31u || q.ui32CmdSize!=sizeof(RGXFWIF_FWCCB_CMD) ||
        q.ui32ReadOffset!=q.ui32WriteOffset || q.ui32ReadOffset>31u;
}
static void publish(struct a04_vendor_start *s,struct a04_rgx_fw_runtime *f,uint32_t next)
{
    a04_gpu_dma_barrier();((volatile RGXFWIF_CCB_CTL *)f->kctl)->ui32WriteOffset=next;
    a04_gpu_dma_barrier();WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+0xb00),0x10);
}
static NTSTATUS wait_kernel(struct a04_rgx_fw_runtime *f,uint32_t next)
{
    RGXFWIF_CCB_CTL k;unsigned i;
    for(i=0;i<20000u;i++){
        if(read_queue(f,&k))return STATUS_IO_DEVICE_ERROR;
        if(k.ui32ReadOffset==next)return STATUS_SUCCESS;
        KeStallExecutionProcessor(50);
    }
    return STATUS_IO_TIMEOUT;
}
NTSTATUS a04_vendor_native_publish_candidate(struct a04_vendor_native_cold *n,
    struct a04_vendor_start *s,struct a04_vendor_native_result *out)
{
    struct a04_vendor_render_cold *r;struct a04_vendor_native_candidate *c;
    struct a04_rgx_fw_runtime *f;RGXFWIF_CCB_CTL k;RGXFWIF_CCCB_CTL ctl[2];
    RGXFWIF_KCCB_CMD flush={0},fence={0};RGXFWIF_CLEANUP_CTL clean[2];
    UCHAR *bank;uint32_t sync[2],next,second,seq;unsigned i,j;NTSTATUS status;
    _Alignas(8) uint32_t pixels[64];
    if(!out)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(out,sizeof(*out));
    if(!n || !s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !(r=n->render) ||
       r->native_owner!=n || !(c=n->candidate) || n->quarantined || r->poisoned ||
       r->cleanup_started || c->attempted || !n->mapped || !n->prepared || !n->bank_prepared ||
       !n->backing_staged || !n->output_mapped || n->code_mapped!=3u ||
       n->width!=800u || n->height!=600u || !n->output[0].cpu || n->output[0].bytes!=A04_NATIVE_OUTPUT_BYTES ||
       !s->online || !s->core || s->memory!=r->firmware ||
       !s->memory || !s->memory->hardware_owned || !s->memory->uncached ||
       !(f=s->memory->runtime) || !f->kctl || !f->fctl || !f->kcommands || !f->command_sync ||
       s->command_sequence>=0xfffffffeu)return STATUS_INVALID_DEVICE_STATE;
    /* The external caller must hold the existing provider lifetime lock. */
    if(InterlockedCompareExchange(&s->command_busy,1,0))return STATUS_DEVICE_BUSY;
    status=STATUS_INVALID_DEVICE_STATE;
    if(r->poisoned || r->cleanup_started || n->quarantined || c->attempted ||
       s->command_poisoned || s->shutdown_started)goto done;
    bank=r->buffer[0].cpu;
    if(read_queue(f,&k) || k.ui32ReadOffset!=k.ui32WriteOffset)goto done;
    /* Reserve both KCCB slots while command_busy excludes other producers. */
    next=(k.ui32WriteOffset+1u)&127u;second=(next+1u)&127u;
    if(next==k.ui32ReadOffset || second==k.ui32ReadOffset)goto done;
    for(i=0;i<2;i++){
        if(r->client_write[i] || a04_rgx_device_read_words(&ctl[i],bank+r->layout.control[i],sizeof(ctl[i])) ||
           ctl[i].ui32WrapMask!=4095u || ctl[i].ui32ReadOffset || ctl[i].ui32WriteOffset || ctl[i].ui32DepOffset)
            goto done;
        /* Reject any context-priority command or overwrite since cold stage. */
        for(j=0;j<4096u;j+=sizeof(pixels)){
            if(a04_rgx_device_read_words(pixels,bank+r->layout.commands[i]+j,sizeof(pixels)))goto done;
            for(unsigned b=0;b<sizeof(pixels);b++)if(((UCHAR *)pixels)[b]!=c->rings[i*4096u+j+b])goto done;
        }
        if(a04_rgx_device_read_words(&clean[i],bank+(i?n->addresses.three_d_cleanup[0]:n->addresses.ta_cleanup[0])-0xc0080000u,sizeof(clean[i])) ||
           clean[i].ui32SubmittedCommands || clean[i].ui32ExecutedCommands)goto done;
    }
    if(a04_rgx_device_read_words(sync,bank+0x4c00u,sizeof(sync)) || sync[0] || sync[1])goto done;
    c->attempted=TRUE;out->phase=1;
    if(a04_rgx_device_write_words(&f->kcommands[k.ui32WriteOffset],&c->pair.ta_kick,sizeof(c->pair.ta_kick)) ||
       a04_rgx_device_write_words(&f->kcommands[next],&c->pair.three_d_kick,sizeof(c->pair.three_d_kick)))goto poison;
    r->client_write[0]=c->pair.ta_write;r->client_write[1]=c->pair.three_d_write;
    publish(s,f,next);publish(s,f,second);out->phase=2;
    status=wait_kernel(f,second);if(!NT_SUCCESS(status))goto poison;
    status=STATUS_IO_TIMEOUT;
    for(i=0;i<20000u;i++){
        if(read_queue(f,&k) || a04_rgx_device_read_words(sync,bank+0x4c00u,sizeof(sync)))goto poison;
        for(j=0;j<2;j++)if(a04_rgx_device_read_words(&ctl[j],bank+r->layout.control[j],sizeof(ctl[j])) ||
            a04_rgx_device_read_words(&clean[j],bank+(j?n->addresses.three_d_cleanup[0]:n->addresses.ta_cleanup[0])-0xc0080000u,sizeof(clean[j])))goto poison;
        for(j=0;j<2;j++)if(ctl[j].ui32WrapMask!=4095u || ctl[j].ui32DepOffset>4095u || (ctl[j].ui32DepOffset&7u))goto poison;
        if(sync[0]==1u && sync[1]==1u && ctl[0].ui32ReadOffset==224u && ctl[0].ui32WriteOffset==224u &&
           ctl[1].ui32ReadOffset==472u && ctl[1].ui32WriteOffset==472u &&
           clean[0].ui32SubmittedCommands==1u && clean[0].ui32ExecutedCommands==1u &&
           clean[1].ui32SubmittedCommands==1u && clean[1].ui32ExecutedCommands==1u){status=STATUS_SUCCESS;break;}
        KeStallExecutionProcessor(50);
    }
    if(!NT_SUCCESS(status))goto poison;
    out->phase=3;
    if(read_queue(f,&k) || k.ui32ReadOffset!=k.ui32WriteOffset)goto poison;
    flush.eCmdType=RGXFWIF_KCCB_CMD_SLCFLUSHINVAL;flush.eDM=0;flush.uCmdData.sSLCFlushInvalData.bInval=TRUE;
    if(a04_rgx_device_write_words(&f->kcommands[k.ui32WriteOffset],&flush,sizeof(flush)))goto poison;
    next=(k.ui32WriteOffset+1u)&127u;publish(s,f,next);
    status=wait_kernel(f,next);if(!NT_SUCCESS(status))goto poison;
    if(read_queue(f,&k) || k.ui32ReadOffset!=k.ui32WriteOffset)goto poison;
    seq=++s->command_sequence;*(volatile uint32_t *)f->command_sync=0;
    fence.eCmdType=RGXFWIF_KCCB_CMD_SYNC;fence.eDM=0;
    fence.uCmdData.sSyncData.sSyncObjDevVAddr=f->command_sync_addr;fence.uCmdData.sSyncData.uiUpdateVal=seq;
    if(a04_rgx_device_write_words(&f->kcommands[k.ui32WriteOffset],&fence,sizeof(fence)))goto poison;
    next=(k.ui32WriteOffset+1u)&127u;publish(s,f,next);
    status=wait_kernel(f,next);if(!NT_SUCCESS(status))goto poison;
    status=STATUS_IO_TIMEOUT;
    for(i=0;i<20000u;i++){
        a04_gpu_dma_barrier();if(*(volatile uint32_t *)f->command_sync==seq){status=STATUS_SUCCESS;break;}
        KeStallExecutionProcessor(50);
    }
    if(!NT_SUCCESS(status))goto poison;
    for(i=0;i<A04_NATIVE_OUTPUT_BYTES/4u;i+=64u){
        if(a04_rgx_device_read_words(pixels,(UCHAR *)n->output[0].cpu+i*4u,sizeof(pixels)))goto poison;
        for(j=0;j<64u;j++)if(i+j<480000u){
            if(pixels[j]==0xff332211u)out->matching_pixels++;else out->mismatched_pixels++;
        }else if(pixels[j]!=0xa5a5a5a5u)out->guard_errors++;
    }
    if(out->mismatched_pixels || out->guard_errors || out->matching_pixels!=480000u)goto poison;
    c->verified=TRUE;out->phase=4;status=STATUS_SUCCESS;goto done;
poison:
    if(NT_SUCCESS(status) || status==STATUS_INVALID_DEVICE_STATE)status=STATUS_IO_DEVICE_ERROR;
    r->poisoned=TRUE;s->command_poisoned=TRUE;n->quarantined=TRUE;
done:
    InterlockedExchange(&s->command_busy,0);return status;
}

NTSTATUS a04_vendor_native_run_once(struct a04_vendor_native_cold *n,
    struct a04_vendor_start *s,struct a04_vendor_native_result *result)
{
    NTSTATUS status;
    if(!n || !s || !result || !n->candidate || n->candidate->attempted)return STATUS_INVALID_DEVICE_STATE;
    status=a04_vendor_native_publish_candidate(n,s,result);
    if(!NT_SUCCESS(status)){
        /* A failed admission is terminal for this diagnostic instance too. */
        n->candidate->attempted=TRUE;n->quarantined=TRUE;s->command_poisoned=TRUE;
        if(n->render)n->render->poisoned=TRUE;
    }
    return status;
}
