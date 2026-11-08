/* SPDX-License-Identifier: MIT
 * Bounded private candidate publisher. NativeClearJob alone remains cold;
 * only the separately default-OFF activation flag adds the D0 caller.
 * No IOCTL or service admits arbitrary GPU packet bytes.
 */
#include "windows_native_job_private.h"
#include "windows_native_submit.h"
#if A04_GE8300_NATIVE_OWNER_MDL_TARGET
#include "native_owner_mdl.h"
#include "native_owner_irq.h"
#endif
#include "windows_start.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
#include "../../windows_hw.h"
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
/* Typed m1.11 MMUCACHE transaction for the retained render memory context.
 * The same 0x1f PT/PD/PC/TLB flags are defined by stock rgxmem.c for the
 * existing non-SLC_VIVT BVNC. Real sync acknowledgement is mandatory. */
static NTSTATUS target_handoff(struct a04_vendor_native_cold *n,struct a04_vendor_start *s,BOOLEAN cpu)
{
#if A04_GE8300_NATIVE_OWNER_MDL_TARGET
    struct a04_rgx_fw_runtime *f=s->memory->runtime;RGXFWIF_CCB_CTL k;
    RGXFWIF_KCCB_CMD cmd={0};NTSTATUS status;uint32_t sequence,next;unsigned i;
    if(!n->registered_target)return STATUS_SUCCESS;
    if(KeGetCurrentIrql()!=PASSIVE_LEVEL || s->command_busy!=1 || s->command_sequence>=65534u ||
       read_queue(f,&k) || k.ui32ReadOffset!=k.ui32WriteOffset ||
       *(volatile uint32_t *)f->command_sync!=s->command_sequence)return STATUS_INVALID_DEVICE_STATE;
    status=cpu?a04_vendor_native_target_remove_pt(n):a04_vendor_native_target_acquire_pt_quiescent(n);
    if(!NT_SUCCESS(status))return status;
    sequence=++s->command_sequence;
    cmd.eCmdType=RGXFWIF_KCCB_CMD_MMUCACHE;cmd.eDM=0;
    cmd.uCmdData.sMMUCacheData.sMemoryContext.ui32Addr=0xc0080000u+n->render->layout.memory;
    cmd.uCmdData.sMMUCacheData.ui32Flags=0x1fu;
    cmd.uCmdData.sMMUCacheData.sMMUCacheSync=f->command_sync_addr;
    cmd.uCmdData.sMMUCacheData.ui16MMUCacheSyncUpdateValue=(uint16_t)sequence;
    *(volatile uint32_t *)f->command_sync=0;
    if(a04_rgx_device_write_words(&f->kcommands[k.ui32WriteOffset],&cmd,sizeof(cmd)))return STATUS_IO_DEVICE_ERROR;
    next=(k.ui32WriteOffset+1u)&127u;publish(s,f,next);
    status=wait_kernel(f,next);if(!NT_SUCCESS(status))return status;
    for(i=0;i<20000u;i++){
        a04_gpu_dma_barrier();
        if(*(volatile uint32_t *)f->command_sync==sequence)
            return cpu?a04_vendor_native_target_cpu_after_mmu_ack(n):STATUS_SUCCESS;
        KeStallExecutionProcessor(50);
    }
    return STATUS_IO_TIMEOUT; /* retain all mappings on uncertain invalidate */
#else
    (void)n;(void)s;(void)cpu;return STATUS_SUCCESS;
#endif
}
#if A04_GE8300_NATIVE_OWNER_MDL_TARGET
static BOOLEAN target_dma_ready(struct a04_vendor_native_cold *n)
{
    ULONG count;
    if(!n || !n->registered_target || !a04_native_mdl_pages(n->registered_target,&count))return FALSE;
    if(n->registered_vertex && !a04_native_mdl_pages(n->registered_vertex,&count))return FALSE;
    for(ULONG i=0;i<n->linear_count;i++)if(!a04_native_mdl_pages(n->linear[i].binding,&count))return FALSE;
    return TRUE;
}
#endif
/* Shared PASSIVE finish: actual TA/3D/UFO/cleanup -> SLC/GP -> MDL cache
 * -> pixel/guard proof. This returns CPU-side state only; it NEVER emits a
 * Dxgk completion or invents a subsequent hardware interrupt. */
static NTSTATUS finish_published(struct a04_vendor_native_cold *n,
    struct a04_vendor_start *s,struct a04_vendor_native_result *out,uint32_t second)
{
    struct a04_vendor_render_cold *r=n->render;
    struct a04_vendor_native_candidate *c=n->candidate;
    struct a04_rgx_fw_runtime *f=s->memory->runtime;
    RGXFWIF_CCB_CTL k;RGXFWIF_CCCB_CTL ctl[2];RGXFWIF_CLEANUP_CTL clean[2];
    RGXFWIF_KCCB_CMD flush={0},fence={0};
    UCHAR *bank=r->buffer[0].cpu;uint32_t sync[2],next,seq;
    unsigned i,j;NTSTATUS status;_Alignas(8) uint32_t pixels[64];
    out->phase=2;
    status=wait_kernel(f,second);if(!NT_SUCCESS(status))goto poison;
    status=STATUS_IO_TIMEOUT;
    for(i=0;i<20000u;i++){
        if(read_queue(f,&k) || a04_rgx_device_read_words(sync,bank+0x4c00u,sizeof(sync)))goto poison;
        for(j=0;j<2;j++)if(a04_rgx_device_read_words(&ctl[j],bank+r->layout.control[j],sizeof(ctl[j])) ||
            a04_rgx_device_read_words(&clean[j],bank+(j?n->addresses.three_d_cleanup[0]:n->addresses.ta_cleanup[0])-0xc0080000u,sizeof(clean[j])))goto poison;
        for(j=0;j<2;j++)if(ctl[j].ui32WrapMask!=4095u || ctl[j].ui32DepOffset>4095u || (ctl[j].ui32DepOffset&7u))goto poison;
        if(sync[0]==c->completed_frames+1u && sync[1]==c->completed_frames+1u && ctl[0].ui32ReadOffset==c->pair.ta_write && ctl[0].ui32WriteOffset==c->pair.ta_write &&
           ctl[1].ui32ReadOffset==c->pair.three_d_write && ctl[1].ui32WriteOffset==c->pair.three_d_write &&
           clean[0].ui32SubmittedCommands==c->completed_frames+1u && clean[0].ui32ExecutedCommands==c->completed_frames+1u &&
           clean[1].ui32SubmittedCommands==c->completed_frames+1u && clean[1].ui32ExecutedCommands==c->completed_frames+1u){status=STATUS_SUCCESS;break;}
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
#if A04_GE8300_NATIVE_OWNER_MDL_TARGET
    status=target_handoff(n,s,TRUE);if(!NT_SUCCESS(status))goto poison;
#endif
    c->scan.started=1;c->scan.frame_index=c->completed_frames;
    for(i=0;i<a04_vendor_native_output_bytes(n)/4u;i+=64u){
        if(a04_vendor_native_storage_read(n,1,i*4u,pixels,sizeof(pixels)))goto poison;
        for(j=0;j<64u;j++){
            uint32_t index=i+j,expected=0xa5a5a5a5u;
            c->scan.scanned_words++;
            if(index<480000u){
#if A04_GE8300_NATIVE_OWNER_MDL_TARGET
                if(c->draw_job){
                    /* Arbitrary triangle coverage is not a fixed-clear
                     * pixel oracle. Real fence/cache and padding guard are
                     * checked; do not fabricate matching_pixels. */
                    c->scan.scanned_pixels++;continue;
                }
#endif
                uint32_t colored;
#if A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
                expected=a04_m111_triangle_expected_pixel(c->completed_frames,index);
                colored=c->completed_frames?0xffcc9933u:0xff6633ccu;
#elif A04_GE8300_NATIVE_OWNER_MDL_TARGET
                expected=colored=c->clear_pixel;
#else
                expected=colored=0xff332211u;
#endif
                c->scan.scanned_pixels++;
                if(pixels[j]==colored)c->scan.colored_pixels++;
                else if(pixels[j]==0xff000000u)c->scan.black_pixels++;
                else c->scan.other_pixels++;
                if(pixels[j]==expected)out->matching_pixels++;else out->mismatched_pixels++;
                /* Fixed five positions. No additional memory read or output
                 * in the scan loop, including on a successful frame. */
                unsigned sample=5;
                switch(index){
                case 0:sample=0;break;
                case 799:sample=1;break;
                case 479200:sample=2;break;
                case 479999:sample=3;break;
                case 240400:sample=4;break; /* x400,y300 */
                default:break;
                }
                if(sample<5){
                    c->scan.sample[sample]=(struct a04_vendor_native_scan_sample){index,expected,pixels[j]};
                    c->scan.sample_mask|=1u<<sample;
                }
            }else if(pixels[j]!=expected)out->guard_errors++;
            if(!c->scan.first_valid && pixels[j]!=expected){
                c->scan.first_valid=1;c->scan.first_index=index;
                c->scan.first_expected=expected;c->scan.first_observed=pixels[j];
                c->scan.first_is_guard=index>=480000u;
            }
        }
    }
    if(out->mismatched_pixels || out->guard_errors ||
#if A04_GE8300_NATIVE_OWNER_MDL_TARGET
       (c->draw_job?c->scan.scanned_pixels!=480000u:out->matching_pixels!=480000u)
#else
       out->matching_pixels!=480000u
#endif
       )goto poison;
    c->verified=TRUE;c->completed_frames++;out->completed_frames=c->completed_frames;out->phase=4;status=STATUS_SUCCESS;goto done;
poison:
    if(NT_SUCCESS(status) || status==STATUS_INVALID_DEVICE_STATE)status=STATUS_IO_DEVICE_ERROR;
    r->poisoned=TRUE;s->command_poisoned=TRUE;n->quarantined=TRUE;
done:
    if(c->scan.started && !NT_SUCCESS(status))c->scan.failed=1;
    InterlockedExchange(&s->command_busy,0);return status;
}

NTSTATUS a04_vendor_native_publish_candidate(struct a04_vendor_native_cold *n,
    struct a04_vendor_start *s,struct a04_vendor_native_result *out)
{
    struct a04_vendor_render_cold *r;struct a04_vendor_native_candidate *c;
    struct a04_rgx_fw_runtime *f;RGXFWIF_CCB_CTL k;RGXFWIF_CCCB_CTL ctl[2];
    RGXFWIF_CLEANUP_CTL clean[2];
    UCHAR *bank;uint32_t sync[2],next,second;unsigned i,j;NTSTATUS status;
    _Alignas(8) uint32_t pixels[64];
    if(!out)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(out,sizeof(*out));
    if(!n || !s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !(r=n->render) ||
       r->native_owner!=n || !(c=n->candidate) || n->quarantined || r->poisoned ||
       r->cleanup_started || c->attempted || !n->mapped || !n->prepared || !n->bank_prepared ||
       !n->backing_staged || !n->output_mapped || n->code_mapped!=3u ||
       n->width!=800u || n->height!=600u || !a04_vendor_native_output_ready(n) ||
       !s->online || !s->core || s->memory!=r->firmware ||
       !s->memory || !s->memory->hardware_owned || !s->memory->uncached ||
       !(f=s->memory->runtime) || !f->kctl || !f->fctl || !f->kcommands || !f->command_sync ||
       s->command_sequence>=0xfffffffeu)return STATUS_INVALID_DEVICE_STATE;
    out->completed_frames=c->completed_frames;
    /* The external caller must hold the existing provider lifetime lock. */
    if(InterlockedCompareExchange(&s->command_busy,1,0))return STATUS_DEVICE_BUSY;
    RtlZeroMemory(&c->scan,sizeof(c->scan));
    status=STATUS_INVALID_DEVICE_STATE;
    if(r->poisoned || r->cleanup_started || n->quarantined || c->attempted ||
       s->command_poisoned || s->shutdown_started)goto done;
    bank=r->buffer[0].cpu;
    if(read_queue(f,&k) || k.ui32ReadOffset!=k.ui32WriteOffset)goto done;
    /* Reserve both KCCB slots while command_busy excludes other producers. */
    next=(k.ui32WriteOffset+1u)&127u;second=(next+1u)&127u;
    if(next==k.ui32ReadOffset || second==k.ui32ReadOffset)goto done;
    for(i=0;i<2;i++){
        if(r->client_write[i]!=c->start[i] || a04_rgx_device_read_words(&ctl[i],bank+r->layout.control[i],sizeof(ctl[i])) ||
           ctl[i].ui32WrapMask!=4095u || ctl[i].ui32ReadOffset!=c->start[i] || ctl[i].ui32WriteOffset!=c->start[i] ||
           ctl[i].ui32DepOffset>4095u || (ctl[i].ui32DepOffset&7u))
            goto done;
        /* Reject any context-priority command or overwrite since cold stage. */
        for(j=0;j<4096u;j+=sizeof(pixels)){
            if(a04_rgx_device_read_words(pixels,bank+r->layout.commands[i]+j,sizeof(pixels)))goto done;
            for(unsigned b=0;b<sizeof(pixels);b++)if(((UCHAR *)pixels)[b]!=c->rings[i*4096u+j+b])goto done;
        }
        if(a04_rgx_device_read_words(&clean[i],bank+(i?n->addresses.three_d_cleanup[0]:n->addresses.ta_cleanup[0])-0xc0080000u,sizeof(clean[i])) ||
           clean[i].ui32SubmittedCommands!=c->completed_frames || clean[i].ui32ExecutedCommands!=c->completed_frames)goto done;
    }
    if(a04_rgx_device_read_words(sync,bank+0x4c00u,sizeof(sync)) || sync[0]!=c->completed_frames || sync[1]!=c->completed_frames)goto done;
    c->attempted=TRUE;out->phase=1;
    status=a04_vendor_native_submit_prepared_pair(n,s,k.ui32WriteOffset);
    if(!NT_SUCCESS(status))goto poison;
    return finish_published(n,s,out,second);
poison:
    if(NT_SUCCESS(status) || status==STATUS_INVALID_DEVICE_STATE)status=STATUS_IO_DEVICE_ERROR;
    r->poisoned=TRUE;s->command_poisoned=TRUE;n->quarantined=TRUE;
done:
    InterlockedExchange(&s->command_busy,0);return status;
}

#if A04_GE8300_NATIVE_OWNER_MDL_TARGET
/* Caller owns the real Dxgk interrupt lock and the retained D0 lease. No
 * allocation, waiting, cache operation, callback, or CPU visibility claim. */
NTSTATUS a04_vendor_native_owner_submit(struct a04_vendor_native_cold *n,
    struct a04_vendor_start *s,uint32_t expected_kernel_write)
{
    struct a04_vendor_render_cold *r;struct a04_vendor_native_candidate *c;
    struct a04_rgx_fw_runtime *f;RGXFWIF_CCB_CTL k;RGXFWIF_CCCB_CTL ctl;
    RGXFWIF_CLEANUP_CTL clean;uint32_t sync[2];NTSTATUS status=STATUS_DEVICE_BUSY;
    if(!n || !s || KeGetCurrentIrql()<DISPATCH_LEVEL || expected_kernel_write>127u ||
       !(r=n->render) || r->native_owner!=n || !(c=n->candidate) ||
       !n->registered_target || !target_dma_ready(n) || !a04_vendor_native_output_ready(n) ||
       !n->mapped || !n->prepared || !n->bank_prepared || !n->backing_staged ||
       !n->output_mapped || n->code_mapped!=3u || n->quarantined || r->poisoned ||
       r->cleanup_started || c->attempted || c->published || c->verified || c->owner_pending ||
       !s->online || !s->core || s->memory!=r->firmware || !s->memory ||
       !s->memory->hardware_owned || !s->memory->uncached ||
       s->command_poisoned || s->shutdown_started ||
       !(f=s->memory->runtime) || !f->kctl || !f->fctl || !f->kcommands ||
       !f->command_sync || s->command_sequence>=0xfffffffeu)return STATUS_INVALID_DEVICE_STATE;
    if(InterlockedCompareExchange(&s->command_busy,1,0))return STATUS_DEVICE_BUSY;
    if(read_queue(f,&k) || k.ui32ReadOffset!=expected_kernel_write ||
       k.ui32WriteOffset!=expected_kernel_write)goto reject;
    for(unsigned i=0;i<2;i++){
        uint32_t cleanup=i?n->addresses.three_d_cleanup[0]:n->addresses.ta_cleanup[0];
        if(r->client_write[i]!=c->start[i] ||
           a04_rgx_device_read_words(&ctl,(UCHAR *)r->buffer[0].cpu+r->layout.control[i],sizeof(ctl)) ||
           ctl.ui32WrapMask!=4095u || ctl.ui32ReadOffset!=c->start[i] ||
           ctl.ui32WriteOffset!=c->start[i] || ctl.ui32DepOffset>4095u ||
           (ctl.ui32DepOffset&7u) ||
           a04_rgx_device_read_words(&clean,(UCHAR *)r->buffer[0].cpu+cleanup-0xc0080000u,sizeof(clean)) ||
           clean.ui32SubmittedCommands!=c->completed_frames ||
           clean.ui32ExecutedCommands!=c->completed_frames)goto reject;
    }
    if(a04_rgx_device_read_words(sync,(UCHAR *)r->buffer[0].cpu+0x4c00u,sizeof(sync)) ||
       sync[0]!=c->completed_frames || sync[1]!=c->completed_frames)goto reject;
    RtlZeroMemory(&c->scan,sizeof(c->scan));
    c->attempted=TRUE;c->owner_pending=TRUE;
    c->owner_second=(expected_kernel_write+2u)&127u;
    status=a04_vendor_native_submit_prepared_pair(n,s,expected_kernel_write);
    if(NT_SUCCESS(status))return status; /* busy remains held through finish */
    n->quarantined=TRUE;r->poisoned=TRUE;s->command_poisoned=TRUE;
reject:
    InterlockedExchange(&s->command_busy,0);return status;
}
NTSTATUS a04_vendor_native_owner_finish_passive(struct a04_vendor_native_cold *n,
    struct a04_vendor_start *s,struct a04_vendor_native_result *out)
{
    struct a04_vendor_native_candidate *c;NTSTATUS status;
    if(!out)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(out,sizeof(*out));
    if(!n || !s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !(c=n->candidate) ||
       !n->registered_target || !n->render || n->render->native_owner!=n ||
       n->quarantined || n->render->poisoned || !c->owner_pending ||
       !c->attempted || !c->published || c->verified || s->command_busy!=1 ||
       !s->memory || s->memory!=n->render->firmware || !s->memory->hardware_owned ||
       !s->memory->uncached || !s->memory->runtime || !s->online ||
       s->command_poisoned || s->shutdown_started)return STATUS_INVALID_DEVICE_STATE;
    out->completed_frames=c->completed_frames;
    status=finish_published(n,s,out,c->owner_second);
    if(NT_SUCCESS(status))c->owner_pending=FALSE;
    return status;
}
/* No render task is added. A fresh GP fence after cache/readback supplies a
 * new hardware-notification opportunity. Lack of a real source stays pending. */
NTSTATUS a04_vendor_native_owner_arm_visibility(struct a04_vendor_native_cold *n,
    struct a04_vendor_start *s,struct a04_native_irq_core *q,uint64_t epoch,
    uint64_t job,uint32_t generation)
{
    struct a04_vendor_native_candidate *c;struct a04_rgx_fw_runtime *f;
    RGXFWIF_CCB_CTL k;RGXFWIF_KCCB_CMD cmd={0};NTSTATUS status;uint32_t seq,next;
    if(!n || !s || !q || KeGetCurrentIrql()<DISPATCH_LEVEL || !epoch || !job || !generation ||
       !(c=n->candidate) || !n->registered_target || !n->render ||
       n->render->native_owner!=n || n->quarantined || n->render->poisoned ||
       !c->verified || !c->attempted || !c->published || c->owner_pending ||
       c->visibility_armed || c->visibility_delivered ||
       s->memory!=n->render->firmware || !s->memory || !s->memory->hardware_owned ||
       !s->memory->uncached || !(f=s->memory->runtime) ||
       !f->command_sync || !s->online || s->command_poisoned || s->shutdown_started ||
       q->core!=s->core || q->runtime!=f || q->epoch!=epoch || !q->powered ||
       !q->masked || q->closed || q->faulted || q->serial==UINT64_MAX ||
       s->command_sequence>=0xfffffffeu)return STATUS_INVALID_DEVICE_STATE;
    if(InterlockedCompareExchange(&s->command_busy,1,0))return STATUS_DEVICE_BUSY;
    status=STATUS_DEVICE_BUSY;
    if(read_queue(f,&k) || k.ui32ReadOffset!=k.ui32WriteOffset ||
       *(volatile uint32_t *)f->command_sync!=s->command_sequence)goto done;
    seq=s->command_sequence+1u;next=(k.ui32WriteOffset+1u)&127u;
    status=a04_native_irq_rearm(q,epoch,q->serial);
    if(!NT_SUCCESS(status)){if(q->faulted)goto poison;goto done;}
    c->owner_irq=q;c->owner_epoch=epoch;c->owner_job=job;c->owner_generation=generation;
    c->visibility_serial=q->serial;c->visibility_fence=seq;c->visibility_armed=TRUE;
    s->command_sequence=seq;*(volatile uint32_t *)f->command_sync=0;
    cmd.eCmdType=RGXFWIF_KCCB_CMD_SYNC;cmd.eDM=0;
    cmd.uCmdData.sSyncData.sSyncObjDevVAddr=f->command_sync_addr;
    cmd.uCmdData.sSyncData.uiUpdateVal=seq;
    if(a04_rgx_device_write_words(&f->kcommands[k.ui32WriteOffset],&cmd,sizeof(cmd))){
        status=STATUS_IO_DEVICE_ERROR;goto poison;
    }
    publish(s,f,next);status=STATUS_SUCCESS;goto done;
poison:
    n->quarantined=TRUE;n->render->poisoned=TRUE;s->command_poisoned=TRUE;
    (void)a04_native_irq_mask_close(q,epoch); /* masking is NOT DMA stop */
done:
    InterlockedExchange(&s->command_busy,0);return status;
}
BOOLEAN a04_vendor_native_owner_interrupt_event(struct a04_vendor_native_cold *n,
    struct a04_vendor_start *s,struct a04_native_irq_core *q,struct a04_native_owner_event *out)
{
    struct a04_native_irq_receipt receipt;struct a04_vendor_native_candidate *c;
    struct a04_rgx_fw_runtime *f;RGXFWIF_CCB_CTL k;NTSTATUS status;
    if(!out)return FALSE;
    RtlZeroMemory(out,sizeof(*out));
    if(!n || !s || !q || KeGetCurrentIrql()<DISPATCH_LEVEL || !(c=n->candidate) ||
       !n->render || s->memory!=n->render->firmware || !s->memory ||
       !(f=s->memory->runtime) || q->runtime!=f || q->core!=s->core)return FALSE;
    if(!a04_native_irq_claim(q,&receipt))return FALSE;
    out->claimed=TRUE;out->irq_serial=receipt.serial;out->status=receipt.fault;
    if(!NT_SUCCESS(receipt.fault))goto poison;
    if(q->draining)return TRUE; /* shutdown IRQ count only, never job completion */
    /* Pending receipts deliberately have job=epoch=generation=0. The outer
     * ISR may queue actual deferred service, never a DMA completion. */
    if(!c->visibility_armed || c->visibility_delivered || c->owner_irq!=q ||
       c->owner_epoch!=receipt.epoch || receipt.serial<=c->visibility_serial ||
       !c->verified || c->owner_pending || !n->registered_target ||
       n->quarantined || n->render->poisoned || s->command_poisoned ||
       s->shutdown_started || !s->online || !s->memory->hardware_owned ||
       !s->memory->uncached || s->command_busy)return TRUE;
    a04_gpu_dma_barrier();
    if(*(volatile uint32_t *)f->command_sync!=c->visibility_fence ||
       read_queue(f,&k) || k.ui32ReadOffset!=k.ui32WriteOffset)return TRUE;
    status=a04_native_irq_rearm(q,receipt.epoch,receipt.serial);
    if(!NT_SUCCESS(status)){out->status=status;if(q->faulted)goto poison;return TRUE;}
    c->visibility_armed=FALSE;c->visibility_delivered=TRUE;
    out->epoch=c->owner_epoch;out->job=c->owner_job;out->generation=c->owner_generation;
    out->completed=TRUE;out->status=STATUS_SUCCESS;return TRUE;
poison:
    n->quarantined=TRUE;n->render->poisoned=TRUE;s->command_poisoned=TRUE;
    return TRUE;
}

#endif

BOOLEAN a04_vendor_native_failed_scan(const struct a04_vendor_native_cold *n,
    struct a04_vendor_native_scan_diagnostic *out)
{
    if(!n || !out || !n->candidate || !n->candidate->scan.started ||
       !n->candidate->scan.failed)return FALSE;
    *out=n->candidate->scan;return TRUE;
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

/* Same-version native_pair supports live idle ring offsets and previous UFO
 * values. capture05 observed A/B/A/B HWRT reuse after completed frames. This
 * serial candidate reuses its owned A only after full TA/3D+flush/GP proof.
 * It never resets FW-owned HWRT, PM freelists, or cleanup counters. */
static NTSTATUS rearm_clear(struct a04_vendor_native_cold *n,struct a04_vendor_start *s)
{
    struct a04_vendor_native_candidate *c=n->candidate;struct a04_vendor_render_cold *r=n->render;
    struct a04_rgx_fw_runtime *f=s->memory->runtime;RGXFWIF_CCB_CTL k;RGXFWIF_CCCB_CTL ctl[2];
    RGXFWIF_CLEANUP_CTL clean;uint32_t sync[2];struct a04_m111_pair_input p={0};
    struct a04_m111_shadow_ring rings[2];_Alignas(8) uint32_t words[64],seen[64];
    unsigned i,j;NTSTATUS status=STATUS_INVALID_DEVICE_STATE;
    if(!c->verified || !c->attempted || !c->completed_frames || c->completed_frames>=16u ||
       n->quarantined || r->poisoned || r->cleanup_started || s->command_poisoned ||
       s->shutdown_started || !s->online || !s->memory->hardware_owned)return status;
    if(InterlockedCompareExchange(&s->command_busy,1,0))return STATUS_DEVICE_BUSY;
    if(read_queue(f,&k) || k.ui32ReadOffset!=k.ui32WriteOffset ||
       *(volatile uint32_t *)f->command_sync!=s->command_sequence ||
       a04_rgx_device_read_words(sync,(UCHAR *)r->buffer[0].cpu+0x4c00u,sizeof(sync)) ||
       sync[0]!=c->completed_frames || sync[1]!=c->completed_frames)goto done;
    for(i=0;i<2;i++){
        uint32_t end=i?c->pair.three_d_write:c->pair.ta_write;
        uint32_t cleanup=i?n->addresses.three_d_cleanup[0]:n->addresses.ta_cleanup[0];
        if(r->client_write[i]!=end || a04_rgx_device_read_words(&ctl[i],
           (UCHAR *)r->buffer[0].cpu+r->layout.control[i],sizeof(ctl[i])) ||
           ctl[i].ui32WrapMask!=4095u || ctl[i].ui32ReadOffset!=end || ctl[i].ui32WriteOffset!=end ||
           ctl[i].ui32DepOffset>4095u || (ctl[i].ui32DepOffset&7u) ||
           a04_rgx_device_read_words(&clean,(UCHAR *)r->buffer[0].cpu+cleanup-0xc0080000u,sizeof(clean)) ||
           clean.ui32SubmittedCommands!=c->completed_frames || clean.ui32ExecutedCommands!=c->completed_frames ||
           a04_rgx_device_read_words(c->rings+i*4096u,(UCHAR *)r->buffer[0].cpu+r->layout.commands[i],4096u))goto done;
        rings[i]=(struct a04_m111_shadow_ring){c->rings+i*4096u,end,end};
    }
    p.ta_context=0xc0080000u+r->layout.context;
    p.three_d_context=p.ta_context+(uint32_t)offsetof(RGXFWIF_FWRENDERCONTEXT,s3DContext);
    p.hwrt=n->addresses.hwrt[0];p.ta_sync=0xc0084c00u;p.three_d_sync=0xc0084c04u;
    p.ta_previous_value=c->completed_frames;p.three_d_update_value=c->completed_frames+1u;
    p.external_job=p.internal_job=c->completed_frames+1u;
#if A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
    if(c->completed_frames!=1u)goto done;
    RtlCopyMemory(c->job.ta,c->triangle.record[1].ta,sizeof(c->job.ta));
    RtlCopyMemory(c->job.three,c->triangle.record[1].three,sizeof(c->job.three));
#endif
    for(i=0;i<4;i++)c->job.ta[0x6cu+i]=(uint8_t)(p.ta_previous_value>>(8u*i));
    if(a04_m111_prepare_shadow_pair(&rings[0],&rings[1],&p,c->job.ta,c->job.three,
       (uint8_t *)c+sizeof(*c),8192u,&c->pair))goto done;
    /* Prove each frame changed the full owned target, even for equal colours.
     * No previous frame may satisfy the next frame's pixel check. */
    for(i=0;i<64;i++)words[i]=0xa5a5a5a5u;
    for(i=0;i<a04_vendor_native_output_bytes(n);i+=sizeof(words)){
        if(a04_vendor_native_storage_write(n,1,i,words,sizeof(words)))goto done;
        a04_gpu_dma_barrier();
        if(a04_vendor_native_storage_read(n,1,i,seen,sizeof(seen)))goto done;
        for(j=0;j<64;j++)if(seen[j]!=words[j])goto done;
    }
#if A04_GE8300_NATIVE_OWNER_MDL_TARGET
    if(n->registered_target){status=target_handoff(n,s,FALSE);if(!NT_SUCCESS(status)){n->quarantined=TRUE;r->poisoned=TRUE;s->command_poisoned=TRUE;goto done;}}
#endif
    for(i=0;i<2;i++){
        if(a04_rgx_device_write_words((UCHAR *)r->buffer[0].cpu+r->layout.commands[i],c->rings+i*4096u,4096u))goto done;
        a04_gpu_dma_barrier();
        for(j=0;j<4096u;j+=sizeof(seen)){
            if(a04_rgx_device_read_words(seen,(UCHAR *)r->buffer[0].cpu+r->layout.commands[i]+j,sizeof(seen)))goto done;
            for(unsigned b=0;b<sizeof(seen);b++)if(((UCHAR *)seen)[b]!=c->rings[i*4096u+j+b])goto done;
        }
        c->start[i]=ctl[i].ui32WriteOffset;
    }
    c->attempted=FALSE;c->verified=FALSE;c->published=FALSE;status=STATUS_SUCCESS;
done:
    InterlockedExchange(&s->command_busy,0);return status;
}
NTSTATUS a04_vendor_native_run_clear_frames(struct a04_vendor_native_cold *n,
    struct a04_vendor_start *s,ULONG count,struct a04_vendor_native_result *out)
{
    ULONG i;NTSTATUS status;
    if(!n || !s || !out || !count || count>16u || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
       !n->candidate || n->candidate->attempted || n->candidate->completed_frames)return STATUS_INVALID_DEVICE_STATE;
#if A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
    if(count!=2u)return STATUS_INVALID_PARAMETER;
#endif
    for(i=0;i<count;i++){
        if(i){
            out->phase=0;out->matching_pixels=out->mismatched_pixels=out->guard_errors=0;
            status=rearm_clear(n,s);
            if(!NT_SUCCESS(status)){
                n->quarantined=TRUE;n->candidate->attempted=TRUE;s->command_poisoned=TRUE;
                n->render->poisoned=TRUE;return status;
            }
        }
        status=a04_vendor_native_run_once(n,s,out);if(!NT_SUCCESS(status))return status;
    }
    return STATUS_SUCCESS;
}
