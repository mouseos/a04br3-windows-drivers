/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded m1.11 TQ clear + global SLC flush and readback, derived from XNU
 * A04BR3RogueTransferFill.h and A04BR3RogueReadback.h commit
 * f5d43bf1677955e7e367545b9bbbd6449776cdda. No user-provided GPU addresses.
 */
#include "windows_render_cold.h"
#include "windows_copy_scratch.h"
#include "clear_validate.h"
#include "clear_tile.h"
#include "windows_start.h"
#ifndef A04_GE8300_SHARED_GPU_DIAGNOSTIC
#define A04_GE8300_SHARED_GPU_DIAGNOSTIC 0
#endif
#ifndef A04_GE8300_SHARED_FULL_FRAME
#define A04_GE8300_SHARED_FULL_FRAME 0
#endif
#if A04_GE8300_SHARED_GPU_DIAGNOSTIC && A04_GE8300_SHARED_FULL_FRAME
#include "shared_frame_plan.h"
#endif
#if A04_GE8300_SHARED_GPU_DIAGNOSTIC
#include "windows_shared_tables.h"
#endif
#include "A04BR3RogueCommand.h"
#include "A04BR3RogueDeviceWords.h"
#include "../windows_hw.h"
#include <stddef.h>
enum { TQ_TASK=0x2abc80ccu, CLIENT_UPDATE=0x2abc00d6u };
struct clear_packet { RGXFWIF_CCB_CMD_HEADER task;uint32_t payload[52];
    RGXFWIF_CCB_CMD_HEADER update;RGXFWIF_UFO ufo; };
_Static_assert(sizeof(struct clear_packet)==296,"m1.11 TQ packet ABI");
static NTSTATUS queue_snapshot(struct a04_rgx_fw_runtime *f,RGXFWIF_CCB_CTL *k,RGXFWIF_CCB_CTL *q)
{
    a04_gpu_dma_barrier();
    if(a04_rgx_device_read_words(k,f->kctl,sizeof(*k)) || a04_rgx_device_read_words(q,f->fctl,sizeof(*q)) ||
       k->ui32WrapMask!=127u || k->ui32CmdSize!=sizeof(RGXFWIF_KCCB_CMD) ||
       k->ui32ReadOffset>127u || k->ui32WriteOffset>127u ||
       q->ui32WrapMask!=31u || q->ui32CmdSize!=sizeof(RGXFWIF_FWCCB_CMD) ||
       q->ui32ReadOffset>31u || q->ui32WriteOffset>31u)return STATUS_IO_DEVICE_ERROR;
    if(k->ui32ReadOffset!=k->ui32WriteOffset || q->ui32ReadOffset!=q->ui32WriteOffset)return STATUS_DEVICE_BUSY;
    return STATUS_SUCCESS;
}
static NTSTATUS clear_locked(struct a04_vendor_render_cold *r,
    struct a04_vendor_start *s,struct a04_vendor_clear_result *result, BOOLEAN locked,
    struct a04_vendor_copy_scratch *copy)
{
    struct a04_rgx_fw_runtime *f;struct clear_packet packet={0};
    RGXFWIF_CCB_CTL k,q;RGXFWIF_CCCB_CTL client;RGXFWIF_KCCB_CMD kick={0},flush={0},fence={0};
    volatile unsigned char *bank;volatile uint32_t *sync,*output;uint32_t at,next,knext,seq=0,flushseq=0,value;
    unsigned i;NTSTATUS status;
#if A04_GE8300_SHARED_GPU_DIAGNOSTIC && A04_GE8300_SHARED_FULL_FRAME
    struct a04_tq_ring_plan plan={0}; RGXFWIF_CCB_CMD_HEADER padding={0};
    BOOLEAN shared_wrap=FALSE;
#endif
    if(!result)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(result,sizeof(*result));
    if(!r || !s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !r->mapped || r->cleanup_started || !r->clear_ready || r->clear_attempted ||
       r->poisoned || !s->online || !s->memory || !s->memory->hardware_owned || s->memory!=r->firmware ||
       !(f=s->memory->runtime) || !f->kctl || !f->fctl || !f->kcommands || !f->command_sync || !s->core ||
       !r->buffer[0].cpu || !r->buffer[4].cpu || r->buffer[4].bytes!=65536u ||
       (copy ? (r->screen_surface || r->copy_owner!=copy || copy->render!=r || !copy->prepared || !copy->exposed ||
         !copy->usc_ready || !copy->task_frozen || !copy->source.cpu || !copy->destination.cpu || !copy->shared.cpu ||
         copy->source.bytes!=4096u || copy->destination.bytes!=4096u || copy->shared.bytes!=4096u) :
        r->screen_surface ? (!r->screen_surface->hardware_exposed || !r->screen_surface->pixels.cpu ||
         r->screen_surface->bytes!=4259840u || r->clear_row_bytes!=3328u ||
         r->clear_first!=1248u*3328u+768u*4u) :
         !a04_vendor_clear_layout_valid(r->clear_first,r->clear_row_bytes)) ||
       r->layout.sync>65532u || r->layout.control[3]>65536u-sizeof(client) ||
       r->layout.commands[3]>65536u-4096u || r->layout.tq>65536u-sizeof(RGXFWIF_FWCOMMONCONTEXT) ||
       r->sequence>=0xfffffffeu || s->command_sequence>=0xfffffffeu)return STATUS_INVALID_PARAMETER;
    if(!locked && InterlockedCompareExchange(&s->command_busy,1,0)!=0)return STATUS_DEVICE_BUSY;
    /* Cleanup can begin between the optimistic check and lock acquisition. */
    if(r->cleanup_started || r->clear_attempted || r->poisoned){status=STATUS_INVALID_DEVICE_STATE;goto done;}
    /* A cold copy lease reserves the same TQ context, sync and PDS heap.
     * Never queue a clear while that lease is held. The caller must also
     * serialize copy-owner lifetime with command_busy and PnP callbacks. */
    if((copy && r->copy_owner!=copy) || (!copy && r->copy_owner)){status=STATUS_DEVICE_BUSY;goto done;}
    if(s->command_poisoned || s->shutdown_started){status=STATUS_IO_DEVICE_ERROR;goto done;}
    status=queue_snapshot(f,&k,&q);if(!NT_SUCCESS(status))goto done;
    bank=r->buffer[0].cpu;sync=(volatile uint32_t *)(void *)(bank+r->layout.sync);
    output=(volatile uint32_t *)(r->screen_surface ? r->screen_surface->pixels.cpu : r->buffer[4].cpu);at=r->client_write[3];
    if(at>4095u || (at&7u) ||
       a04_rgx_device_read_words(&client,bank+r->layout.control[3],sizeof(client)) ||
       client.ui32WrapMask!=4095u || client.ui32ReadOffset!=at || client.ui32WriteOffset!=at ||
       client.ui32DepOffset>4095u || (client.ui32DepOffset&7u)) {status=STATUS_INVALID_DEVICE_STATE;goto done;}
#if A04_GE8300_SHARED_GPU_DIAGNOSTIC && A04_GE8300_SHARED_FULL_FRAME
    shared_wrap=copy && (copy->diagnostic_cpu || copy->shared_gpu_only);
    if(shared_wrap) {
        if(!a04_tq_ring_plan(client.ui32ReadOffset,at,client.ui32DepOffset,&plan)) {
            status=STATUS_DEVICE_BUSY;goto done;
        }
    } else
#endif
    if(at+sizeof(packet)+40u>4096u) {status=STATUS_INVALID_DEVICE_STATE;goto done;}
    seq=++r->sequence;packet.task.eCmdType=TQ_TASK;packet.task.ui32CmdSize=208u;
    RtlCopyMemory(packet.payload,copy?copy->frozen_task:r->clear.payload,sizeof(packet.payload));
#if A04_GE8300_SHARED_GPU_DIAGNOSTIC
    if(copy && (copy->diagnostic_cpu || copy->shared_gpu_only)) {
        uint64_t csr=(UINT64_C(2)<<56)|((UINT64_C(799)&UINT64_C(0x1fffe))<<33)|
            (UINT64_C(1)<<23)|(UINT64_C(2)<<26)|(UINT64_C(3)<<29);
        packet.payload[32]=(uint32_t)csr;packet.payload[33]=(uint32_t)(csr>>32);
    }
#endif
#if A04_GE8300_SHARED_WIDE_FRAME
    if(copy && copy->wide_frame) {
        packet.payload[6]=copy->wide_state.tile_size;
        RtlCopyMemory(packet.payload+32,copy->wide_state.csr,sizeof(copy->wide_state.csr));
    }
#endif
    packet.update.eCmdType=CLIENT_UPDATE;packet.update.ui32CmdSize=8;
    packet.ufo.puiAddrUFO.ui32Addr=0xc0080000u+r->layout.sync;packet.ufo.ui32Value=seq;
    *sync=0;
#if A04_GE8300_SHARED_GPU_DIAGNOSTIC && A04_GE8300_SHARED_FULL_FRAME
    if(shared_wrap && plan.pad_bytes) {
        padding.eCmdType=0x2abc00ddu; padding.ui32CmdSize=plan.pad_bytes-sizeof(padding);
        if(a04_rgx_device_write_words(bank+r->layout.commands[3]+plan.pad_at,&padding,sizeof(padding))) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
    }
    if(shared_wrap)at=plan.task_at;
#endif
    if(a04_rgx_device_write_words(bank+r->layout.commands[3]+at,&packet,sizeof(packet))) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
    next=at+sizeof(packet);kick.eCmdType=RGXFWIF_KCCB_CMD_KICK;kick.eDM=3;
    kick.uCmdData.sCmdKickData.psContext.ui32Addr=0xc0080000u+r->layout.tq;
    kick.uCmdData.sCmdKickData.ui32CWoffUpdate=next;
    if(a04_rgx_device_write_words(&f->kcommands[k.ui32WriteOffset],&kick,sizeof(kick))) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
    a04_gpu_dma_barrier();knext=(k.ui32WriteOffset+1u)&127u;
    ((volatile RGXFWIF_CCB_CTL *)f->kctl)->ui32WriteOffset=knext;
    a04_gpu_dma_barrier();r->client_write[3]=next;r->clear_attempted=TRUE;result->phase=1;
    WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+0xb00),0x10);
    status=STATUS_IO_TIMEOUT;
    for(i=0;i<10000;i++) {
        a04_gpu_dma_barrier();
        if(a04_rgx_device_read_words(&client,bank+r->layout.control[3],sizeof(client))) {status=STATUS_IO_DEVICE_ERROR;break;}
        value=*sync;
        if(((volatile RGXFWIF_CCB_CTL *)f->kctl)->ui32ReadOffset==knext &&
           client.ui32ReadOffset==next && client.ui32WriteOffset==next && value==seq) {status=STATUS_SUCCESS;break;}
        KeStallExecutionProcessor(10);
    }
    if(!NT_SUCCESS(status))goto poison;
    result->phase=2;
    status=queue_snapshot(f,&k,&q);if(!NT_SUCCESS(status))goto poison;
    /* Flush the global SLC and fence before sampling Device memory. */
    flushseq=++s->command_sequence;*(volatile uint32_t *)f->command_sync=0;
    flush.eCmdType=RGXFWIF_KCCB_CMD_SLCFLUSHINVAL;flush.eDM=0;
    flush.uCmdData.sSLCFlushInvalData.bInval=TRUE;
    fence.eCmdType=RGXFWIF_KCCB_CMD_SYNC;fence.eDM=0;
    fence.uCmdData.sSyncData.sSyncObjDevVAddr=f->command_sync_addr;
    fence.uCmdData.sSyncData.uiUpdateVal=flushseq;
    /* The board DDK schedules the flush and the subsequent GP fence as
     * separate submissions, each with its own event kick. */
    if(a04_rgx_device_write_words(&f->kcommands[k.ui32WriteOffset],&flush,sizeof(flush))) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
    a04_gpu_dma_barrier();knext=(k.ui32WriteOffset+1u)&127u;
    ((volatile RGXFWIF_CCB_CTL *)f->kctl)->ui32WriteOffset=knext;
    a04_gpu_dma_barrier();result->phase=3;WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+0xb00),0x10);
    status=STATUS_IO_TIMEOUT;
    for(i=0;i<20000;i++) {
        a04_gpu_dma_barrier();
        if(((volatile RGXFWIF_CCB_CTL *)f->kctl)->ui32ReadOffset==knext){status=STATUS_SUCCESS;break;}
        KeStallExecutionProcessor(50);
    }
    if(!NT_SUCCESS(status))goto poison;
    status=queue_snapshot(f,&k,&q);if(!NT_SUCCESS(status))goto poison;
    if(a04_rgx_device_write_words(&f->kcommands[k.ui32WriteOffset],&fence,sizeof(fence))) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
    a04_gpu_dma_barrier();knext=(k.ui32WriteOffset+1u)&127u;
    ((volatile RGXFWIF_CCB_CTL *)f->kctl)->ui32WriteOffset=knext;
    a04_gpu_dma_barrier();result->phase=5;WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+0xb00),0x10);
    status=STATUS_IO_TIMEOUT;
    /* MTK m1.11 sysinfo.h permits at least 1 s for hardware completion. */
    for(i=0;i<20000;i++) {
        a04_gpu_dma_barrier();
        if(((volatile RGXFWIF_CCB_CTL *)f->kctl)->ui32ReadOffset==knext &&
           *(volatile uint32_t *)f->command_sync==flushseq){status=STATUS_SUCCESS;break;}
        KeStallExecutionProcessor(50);
    }
    if(!NT_SUCCESS(status))goto poison;
    result->phase=4;
    a04_gpu_dma_barrier();
    if(copy) {
        volatile uint32_t *src=copy->source.cpu,*dst=copy->destination.cpu,*shared=copy->shared.cpu;
#if A04_GE8300_SHARED_GPU_DIAGNOSTIC
        if(copy->diagnostic_cpu) {
            KeFlushIoBuffers(copy->diagnostic_mdl,TRUE,TRUE);
            a04_gpu_dma_barrier();
        }
#endif
        /* Retain cold guards for the first three wide frames. These private
         * scratch allocations are not the later wide task's output. Scanning
         * them on every present would be diagnostic bus traffic in the render
         * path. Command/UFO completion, SLC flush and GP fence remain above;
         * descriptor checks and first-three full-frame checks remain outside. */
#if A04_GE8300_SHARED_WIDE_FRAME
        if(!copy->wide_frame || copy->wide_guard_samples<3u)
#endif
        {
        for(i=0;i<1024u;i++) {
            uint32_t expected=copy->service_expected_active ? copy->service_expected[i] : a04_vendor_copy_pattern(i);
            value=dst[i];
#if A04_GE8300_SHARED_GPU_DIAGNOSTIC
            if(copy->diagnostic_cpu) {
                value=copy->diagnostic_cpu[(i/32u)*800u+(i%32u)];
                if(dst[i]!=0xa5a5a5a5u)result->guard_errors++;
            } else if(copy->shared_gpu_only && !copy->shared_frame_interior && dst[i]!=0xa5a5a5a5u)result->guard_errors++;
#endif
            if(!copy->shared_gpu_only && value==expected)result->zeros++;
            if(src[i]!=expected)result->guard_errors++;
            if((!copy->shared_frame_interior || i<9u) &&
               shared[i]!=(i<9u?copy->state.shared[i]:0xa5a5a5a5u))result->guard_errors++;
            if(i==0)result->first=value;
            if(i==1023u)result->last=value;
        }
        /* The general-heap pages immediately preceding source belong to
         * the cold clear scratch; verify their original guard as well. */
        /* These 64 KiB never participate in a shared-frame copy. Scanning
         * Device memory on all 1000 tiles repeats 16 million MMIO reads.
         * Validate both frame boundaries; a corrupted final guard fails the
         * last tile, so the provider cannot seal or accept that frame. */
        if(!copy->shared_frame_interior)
            for(i=0;i<16384u;i++)if(output[i]!=0xa5a5a5a5u)result->guard_errors++;
        }
    } else for(i=0;i<(r->screen_surface ? r->screen_surface->bytes/4u : 16384u);i++) {
        value=output[i];if(a04_vendor_clear_is_target(i*4u,r->clear_first,r->clear_row_bytes)) {if(value==r->clear.expected)result->zeros++;} else if(value!=0xa5a5a5a5u)result->guard_errors++;
        if(i*4u==r->clear_first)result->first=value;
        if(i*4u==r->clear_first+31u*r->clear_row_bytes+124u)result->last=value;
    }
    status=((copy && copy->shared_gpu_only ? TRUE : result->zeros==1024u) && !result->guard_errors)?STATUS_SUCCESS:STATUS_DATA_ERROR;
    r->clear_verified=NT_SUCCESS(status);
    if (r->clear_verified) { result->completed_task=seq; result->completed_flush=flushseq; }
    if(copy && !NT_SUCCESS(status)){r->poisoned=TRUE;s->command_poisoned=TRUE;}
    goto done;
poison:
    /* Snapshot only. A timeout does not establish completion or quiescence. */
    a04_gpu_dma_barrier();
    result->kread=((volatile RGXFWIF_CCB_CTL *)f->kctl)->ui32ReadOffset;
    result->kwrite=((volatile RGXFWIF_CCB_CTL *)f->kctl)->ui32WriteOffset;
    if(!a04_rgx_device_read_words(&client,bank+r->layout.control[3],sizeof(client))) {
        result->cread=client.ui32ReadOffset;result->cwrite=client.ui32WriteOffset;
    }
    result->ufo=(result->phase==3 || result->phase==5) ? *(volatile uint32_t *)f->command_sync : *sync;
    result->expected=(result->phase==3 || result->phase==5) ? flushseq : seq;
    if(result->phase==3 || result->phase==5) {
        /* Diagnostic observation only: the flush is still unconfirmed and
         * reads may race cache/device activity. Never treat this as success. */
        result->slc_status=READ_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+0x3820));
        result->slc_idle=READ_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+0x3898));
        result->provisional=1;
        for(i=0;!copy && i<(r->screen_surface ? r->screen_surface->bytes/4u : 16384u);i++) {
            value=output[i];
            if(a04_vendor_clear_is_target(i*4u,r->clear_first,r->clear_row_bytes)) {if(value==r->clear.expected)result->zeros++;}
            else if(value!=0xa5a5a5a5u)result->guard_errors++;
            if(i*4u==r->clear_first)result->first=value;
            if(i*4u==r->clear_first+31u*r->clear_row_bytes+124u)result->last=value;
        }
    }
    r->poisoned=TRUE;s->command_poisoned=TRUE;
done:
    if(!locked)InterlockedExchange(&s->command_busy,0);
    return status;
}

NTSTATUS a04_vendor_render_clear_once(struct a04_vendor_render_cold *r,
    struct a04_vendor_start *s,struct a04_vendor_clear_result *result)
{ return clear_locked(r,s,result,FALSE,NULL); }
NTSTATUS a04_vendor_render_copy_once(struct a04_vendor_copy_scratch *o,
    struct a04_vendor_start *s,struct a04_vendor_clear_result *result)
{ return clear_locked(o?o->render:NULL,s,result,FALSE,o); }

#if A04_GE8300_SHARED_GPU_DIAGNOSTIC
/* Fixed 32x32 diagnostic into the first pixels of the shared 800x1280
 * surface. The owner holds the provider lifetime lock and a published mapping.
 * Completion + CPU readback do not establish display visibility or quiescence. */
static NTSTATUS copy_shared_tile(struct a04_vendor_copy_scratch *o,
    struct a04_vendor_start *s,struct a04_shared_scanout *target,struct a04_shared_tables *tables,
    const uint32_t *input,uint32_t index,BOOLEAN gpu_only,struct a04_vendor_clear_result *result)
{
    struct a04_vendor_render_cold *r;volatile uint32_t *pixels;uint32_t pds[16];
    unsigned i;NTSTATUS status;uint32_t offset=0;uint64_t gpu_va;
#if A04_GE8300_SHARED_FULL_FRAME
    struct a04_shared_tile tile;
    if(!a04_shared_tile_at(index,&tile))return STATUS_INVALID_PARAMETER;
    offset=tile.byte_offset;
#else
    if(index || input || gpu_only)return STATUS_INVALID_PARAMETER;
#endif
    gpu_va=UINT64_C(0x8000000000)+offset;
    if(!result)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(result,sizeof(*result));
    if(!o || !s || !target || !tables || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
       !tables->published || !tables->mmu_ack || !tables->quarantined || !target->exposed ||
       !target->mdl || !target->gpu_sg || !a04_shared_has_display_backing(target) ||
       !(r=o->render) || r->copy_owner!=o || !o->prepared || !o->exposed ||
       !o->task_frozen || !o->usc_ready || !r->buffer[7].cpu || r->buffer[7].bytes!=65536u ||
       !o->source.cpu || !o->destination.cpu || !o->shared.cpu)return STATUS_INVALID_DEVICE_STATE;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0)return STATUS_DEVICE_BUSY;
    if(r->poisoned || s->command_poisoned || s->shutdown_started || r->cleanup_started ||
       (r->clear_attempted && !r->clear_verified) || !s->online || !s->memory ||
       !s->memory->hardware_owned || r->firmware!=s->memory || !target->gpu_adapter ||
       target->gpu_adapter!=s->memory->adapter ||
       r->client_write[3]>4095u
#if !A04_GE8300_SHARED_FULL_FRAME
       || r->client_write[3]+sizeof(struct clear_packet)+40u>4096u
#endif
       ) {
        status=STATUS_INVALID_DEVICE_STATE;goto out;
    }
    if(a04_rgx_device_read_words(pds,(UCHAR *)r->buffer[7].cpu+64,sizeof(pds))) {
        status=STATUS_IO_DEVICE_ERROR;goto out;
    }
    for(i=0;i<16;i++)if(pds[i]!=o->eot_pds[i]) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto out;}
    pixels=NULL;
    if(!gpu_only) {
        pixels=(volatile uint32_t *)MmGetSystemAddressForMdlSafe(target->mdl,NormalPagePriority);
        if(!pixels || ((uintptr_t)pixels&3u)){status=STATUS_INSUFFICIENT_RESOURCES;goto out;}
        pixels+=offset/4u;
    }
    for(i=0;i<1024u;i++) {
        o->service_expected[i]=input?input[i]:a04_vendor_copy_pattern(i);
        if(pixels)pixels[(i/32u)*800u+(i%32u)]=0xa5a5a5a5u;
        ((volatile uint32_t *)o->destination.cpu)[i]=0xa5a5a5a5u;
    }
    if(pixels)KeFlushIoBuffers(target->mdl,FALSE,TRUE);
    if(a04_rgx_device_write_words(o->source.cpu,o->service_expected,4096)) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
    RtlCopyMemory(pds,o->eot_pds,sizeof(pds));
    pds[2]=(uint32_t)(gpu_va>>2);
    pds[3]=(uint32_t)(gpu_va>>34)|(1u<<19);
    /* Write only the EOT PDS span under the same lock as the normal copy. */
    if(a04_rgx_device_write_words((UCHAR *)r->buffer[7].cpu+64,pds,sizeof(pds))) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
    a04_gpu_dma_barrier();o->service_expected_active=TRUE;o->diagnostic_mdl=pixels?target->mdl:NULL;o->diagnostic_cpu=pixels;o->shared_gpu_only=gpu_only;
    o->shared_frame_interior=gpu_only && index>0u && index<999u;
    r->clear_attempted=FALSE;r->clear_verified=FALSE;
    status=clear_locked(r,s,result,TRUE,o);
    if(result->phase==4) {
        if(a04_rgx_device_write_words((UCHAR *)r->buffer[7].cpu+64,o->eot_pds,sizeof(o->eot_pds))) {
            status=STATUS_IO_DEVICE_ERROR;goto poison;
        }
        a04_gpu_dma_barrier();
    }
    if(!NT_SUCCESS(status))goto poison;
    o->service_expected_active=FALSE;o->diagnostic_mdl=NULL;o->diagnostic_cpu=NULL;o->shared_gpu_only=FALSE;o->shared_frame_interior=FALSE;goto out;
poison:
    /* Retain changed PDS and mappings if completion is uncertain. */
    r->poisoned=TRUE;s->command_poisoned=TRUE;
out:
    InterlockedExchange(&s->command_busy,0);return status;
}

NTSTATUS a04_vendor_render_copy_shared_diagnostic(struct a04_vendor_copy_scratch *o,
    struct a04_vendor_start *s,struct a04_shared_scanout *target,struct a04_shared_tables *tables,
    struct a04_vendor_clear_result *result)
{ return copy_shared_tile(o,s,target,tables,NULL,0,FALSE,result); }
#if A04_GE8300_SHARED_FULL_FRAME
/* Trusted kernel buffer: one packed 32x32 tile, held stable for the call. This
 * is not a frame-completion or display-DMA ownership transfer. */
NTSTATUS a04_vendor_render_copy_shared_tile(struct a04_vendor_copy_scratch *o,
    struct a04_vendor_start *s,struct a04_shared_scanout *target,struct a04_shared_tables *tables,
    const uint32_t *input,uint32_t index,struct a04_vendor_clear_result *result)
{
    if(!input || ((uintptr_t)input&3u))return STATUS_INVALID_PARAMETER;
    return copy_shared_tile(o,s,target,tables,input,index,TRUE,result);
}
#endif
#endif
/* Finite ring experiment: no wrap or teardown is inferred from a fence. */
NTSTATUS a04_vendor_render_copy_buffer_fenced(struct a04_vendor_copy_scratch *o,
    struct a04_vendor_start *s,const void *input,void *output,size_t bytes,
    uint32_t *task,uint32_t *flush)
{
    struct a04_vendor_render_cold *r;struct a04_vendor_clear_result result;
    unsigned i;NTSTATUS status;
    if (!task || !flush || task==flush) return STATUS_INVALID_PARAMETER;
    *task=0;*flush=0;
    if(!o || !s || !input || !output || bytes!=4096u ||
       ((uintptr_t)input&3u) || ((uintptr_t)output&3u) || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0)return STATUS_DEVICE_BUSY;
    r=o->render;
    if(!r || !o->prepared || !o->exposed || !o->usc_ready || !o->task_frozen ||
       !o->source.cpu || !o->destination.cpu || !o->shared.cpu || r->copy_owner!=o ||
       r->poisoned || s->command_poisoned || s->shutdown_started || r->cleanup_started ||
       (r->clear_attempted && !r->clear_verified) || !s->online ||
       !s->memory || !s->memory->hardware_owned || r->firmware!=s->memory) {
        status=STATUS_INVALID_DEVICE_STATE;goto out;
    }
    /* Refuse before touching or exposing buffers when the bounded CCCB has
     * no room; no wrap/PADDING command has been validated for this ABI. */
    if(r->client_write[3]>4095u || r->client_write[3]+sizeof(struct clear_packet)+40u>4096u) {
        status=STATUS_DEVICE_BUSY;goto out;
    }
    RtlCopyMemory(o->service_expected,input,4096u);
    if(a04_rgx_device_write_words(o->source.cpu,o->service_expected,4096u)) {
        status=STATUS_IO_DEVICE_ERROR;goto poison;
    }
    for(i=0;i<1024u;i++) ((volatile uint32_t *)o->destination.cpu)[i]=0xa5a5a5a5u;
    a04_gpu_dma_barrier();o->service_expected_active=TRUE;
    r->clear_attempted=FALSE;r->clear_verified=FALSE;
    status=clear_locked(r,s,&result,TRUE,o);
    if(!NT_SUCCESS(status))goto poison;
    /* Only the completed, synchronized private bounce reaches the caller. */
    if(a04_rgx_device_read_words(output,o->destination.cpu,4096u)) {
        status=STATUS_IO_DEVICE_ERROR;goto poison;
    }
    *task=result.completed_task;*flush=result.completed_flush;
    o->service_expected_active=FALSE;goto out;
poison:
    r->poisoned=TRUE;s->command_poisoned=TRUE;
out:
    InterlockedExchange(&s->command_busy,0);return status;
}

NTSTATUS a04_vendor_render_copy_buffer(struct a04_vendor_copy_scratch *o,
    struct a04_vendor_start *s,const void *input,void *output,size_t bytes)
{
    uint32_t task,flush;
    return a04_vendor_render_copy_buffer_fenced(o,s,input,output,bytes,&task,&flush);
}

/* Internal diagnostic: reuse only after the previous job, SLC flush, fence and
 * full readback all completed. The 64 KiB target and PDS belong to this render
 * object, never to the display. Any failure leaves the sequence closed. */
NTSTATUS a04_vendor_render_next_tile(struct a04_vendor_render_cold *r,
    struct a04_vendor_start *s,unsigned index,struct a04_vendor_clear_result *result)
{
    _Alignas(8) struct a04_vendor_clear_tile tile;
    struct a04_vendor_clear_tile_request q={128,128,128,0,0};
    uint32_t colour;unsigned i;NTSTATUS status;
    if(!result)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(result,sizeof(*result));
    if(!r || !s || (index!=1 && index!=2))return STATUS_INVALID_PARAMETER;
    q.x=index==1?32u:96u;q.y=index==1?32u:96u;
    colour=index==1?0u:0x78563412u;
    if(a04_vendor_clear_tile_build(&tile,&q))return STATUS_INVALID_PARAMETER;
    tile.image.payload[2]=colour;tile.image.expected=colour;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0)return STATUS_DEVICE_BUSY;
    if(r->copy_owner || r->cleanup_started || r->poisoned || s->command_poisoned || s->shutdown_started || !r->mapped ||
       !r->clear_ready || !r->clear_attempted || !r->clear_verified || r->screen_surface || r->diagnostic_tile_index+1!=index ||
       !r->buffer[4].cpu || r->buffer[4].bytes!=65536u || !r->buffer[7].cpu ||
       !s->online || !s->memory || !s->memory->hardware_owned || s->memory!=r->firmware) {
        InterlockedExchange(&s->command_busy,0);return STATUS_INVALID_DEVICE_STATE;
    }
    /* Device mappings are uncached; do not reuse after a failed transfer. */
    for(i=0;i<16384;i++) ((volatile uint32_t *)r->buffer[4].cpu)[i]=0xa5a5a5a5u;
    if(a04_rgx_device_write_words(r->buffer[7].cpu,tile.image.pds,sizeof(tile.image.pds))) {
        r->poisoned=TRUE;s->command_poisoned=TRUE;InterlockedExchange(&s->command_busy,0);
        return STATUS_IO_DEVICE_ERROR;
    }
    r->clear=tile.image;r->clear_first=tile.first;r->clear_row_bytes=tile.row_bytes;
    a04_gpu_dma_barrier();r->clear_attempted=FALSE;r->clear_verified=FALSE;r->diagnostic_tile_index=index;
    status=clear_locked(r,s,result,TRUE,NULL);
    /* No retry on admission or submission failure. */
    if(!NT_SUCCESS(status)) {r->poisoned=TRUE;s->command_poisoned=TRUE;}
    InterlockedExchange(&s->command_busy,0);return status;
}

/* Bounded diagnostic replay of the exact same screen tile and PDS. The caller
 * owns the adapter/render lifetime throughout this call. A completed fence is
 * necessary for reuse but does not prove DMA quiescence at teardown. */
NTSTATUS a04_vendor_render_repeat_screen(struct a04_vendor_render_cold *r,
    struct a04_vendor_start *s,unsigned index,struct a04_vendor_clear_result *result)
{
    struct a04_vendor_surface *screen;unsigned i;NTSTATUS status;
    if(!result)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(result,sizeof(*result));
    if(!r || !s || KeGetCurrentIrql()!=PASSIVE_LEVEL || (index!=1 && index!=2))return STATUS_INVALID_PARAMETER;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0)return STATUS_DEVICE_BUSY;
    screen=r->screen_surface;
    if(r->copy_owner || r->cleanup_started || r->poisoned || s->command_poisoned || s->shutdown_started || !r->mapped ||
       !r->clear_ready || !r->clear_attempted || !r->clear_verified || r->diagnostic_tile_index+1!=index ||
       !screen || !screen->prepared || !screen->hardware_exposed || !screen->pixels.cpu ||
       screen->bytes!=4259840u || screen->pixels.bytes!=screen->bytes ||
       !s->online || !s->memory || !s->memory->hardware_owned || s->memory!=r->firmware) {
        status=STATUS_INVALID_DEVICE_STATE;goto out;
    }
    /* Keep the PDS and all mappings unchanged while the next job uses them. */
    for(i=0;i<screen->bytes/4u;i++)((volatile uint32_t *)screen->pixels.cpu)[i]=0xa5a5a5a5u;
    a04_gpu_dma_barrier();r->clear_attempted=FALSE;r->clear_verified=FALSE;r->diagnostic_tile_index=index;
    status=clear_locked(r,s,result,TRUE,NULL);
    if(!NT_SUCCESS(status)){r->poisoned=TRUE;s->command_poisoned=TRUE;}
out:
    InterlockedExchange(&s->command_busy,0);return status;
}

#if A04_GE8300_SHARED_WIDE_FRAME
/* Lifetime lock held; both complete surfaces and all six tables stay pinned.
 * Nothing here changes a display address or retires a DMA mapping. */
NTSTATUS a04_vendor_render_copy_wide_frame(struct a04_vendor_copy_scratch *o,
    struct a04_vendor_start *s,struct a04_shared_scanout *target,struct a04_shared_tables *tables,
    const uint32_t *input,struct a04_vendor_clear_result *result)
{
    struct a04_vendor_render_cold *r;struct a04_vendor_wide_state wide;
    uint32_t saved_shared[9],actual_shared[9],pds[16];unsigned i,x,y,x0=800,y0=1280,x1=0,y1=0;NTSTATUS status;
    if(!result)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(result,sizeof(*result));
    if(!o || !s || !target || !tables || !input || ((uintptr_t)input&3u) ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL || !tables->published || !tables->mmu_ack ||
       !tables->quarantined || !target->exposed || !target->mdl || !target->gpu_sg ||
       !a04_shared_has_display_backing(target) || !(r=o->render) || r->copy_owner!=o ||
       !o->prepared || !o->exposed || !o->task_frozen || !o->usc_ready ||
       !tables->wide_source.cpu || !tables->wide_shadow || tables->wide_source.bytes!=A04_VENDOR_WIDE_BYTES ||
       !r->buffer[7].cpu || r->buffer[7].bytes!=65536u || r->poisoned || s->command_poisoned ||
       s->shutdown_started || r->cleanup_started || (r->clear_attempted && !r->clear_verified) ||
       !s->online || !s->memory || !s->memory->hardware_owned || r->firmware!=s->memory ||
       target->gpu_adapter!=s->memory->adapter || tables->wide_source.adapter!=target->gpu_adapter)
        return STATUS_INVALID_DEVICE_STATE;
    if(a04_vendor_copy_wide_state_build(&wide,A04_VENDOR_WIDE_SOURCE_VA,800,1280,800))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0)return STATUS_DEVICE_BUSY;
    if(a04_rgx_device_read_words(pds,(UCHAR *)r->buffer[7].cpu+64,sizeof(pds)) ||
       a04_rgx_device_read_words(actual_shared,o->shared.cpu,sizeof(actual_shared))) {
        status=STATUS_IO_DEVICE_ERROR;goto out;
    }
    for(i=0;i<16;i++)if(pds[i]!=o->eot_pds[i]){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto out;}
    for(i=0;i<9;i++)if(actual_shared[i]!=o->state.shared[i]){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto out;}
    RtlCopyMemory(saved_shared,o->state.shared,sizeof(saved_shared));
    /* Compare only cached CPU images. Reading the Device-memory source back
     * would repeat the slow bus access this optimization removes. Keep the
     * proven full-surface GPU task, changing only its CPU source upload. */
    if(!tables->wide_shadow_valid){x0=0;y0=0;x1=800;y1=1280;}
    else for(y=0;y<1280u;y++)for(x=0;x<800u;x++) {
        i=y*800u+x;
        if(input[i]!=tables->wide_shadow[i]) {
            if(x<x0)x0=x;if(y<y0)y0=y;if(x+1u>x1)x1=x+1u;if(y+1u>y1)y1=y+1u;
        }
    }
    tables->last_upload_bytes=0;
    if(x0<x1 && y0<y1)for(y=y0;y<y1;y++) {
        i=y*800u+x0;
        if(a04_rgx_device_write_words((UCHAR *)tables->wide_source.cpu+i*4u,input+i,(x1-x0)*4u)) {
            status=STATUS_IO_DEVICE_ERROR;goto poison;
        }
        tables->last_upload_bytes+=(x1-x0)*4u;
    }
    pds[2]=(uint32_t)(UINT64_C(0x8000000000)>>2);
    pds[3]=(uint32_t)(UINT64_C(0x8000000000)>>34)|(1u<<19);
    o->wide_state=wide;RtlCopyMemory(o->state.shared,wide.shared,sizeof(wide.shared));
    if(a04_rgx_device_write_words(o->shared.cpu,wide.shared,sizeof(wide.shared)) ||
       a04_rgx_device_write_words((UCHAR *)r->buffer[7].cpu+64,pds,sizeof(pds))) {
        status=STATUS_IO_DEVICE_ERROR;goto poison;
    }
    for(i=0;i<1024u;i++)((volatile uint32_t *)o->destination.cpu)[i]=0xa5a5a5a5u;
    o->wide_frame=TRUE;o->shared_gpu_only=TRUE;o->shared_frame_interior=FALSE;
    o->service_expected_active=FALSE;o->diagnostic_mdl=NULL;o->diagnostic_cpu=NULL;
    r->clear_attempted=FALSE;r->clear_verified=FALSE;a04_gpu_dma_barrier();
    status=clear_locked(r,s,result,TRUE,o);
    if(!NT_SUCCESS(status) || result->phase!=4 || !result->completed_task || !result->completed_flush)
        goto poison;
    /* Completion + SLC fence precede restoring the private descriptor/PDS.
     * Retain changed images if either restoration is uncertain. */
    if(a04_rgx_device_write_words((UCHAR *)r->buffer[7].cpu+64,o->eot_pds,sizeof(o->eot_pds)) ||
       a04_rgx_device_write_words(o->shared.cpu,saved_shared,sizeof(saved_shared))) {
        status=STATUS_IO_DEVICE_ERROR;goto poison;
    }
    RtlCopyMemory(o->state.shared,saved_shared,sizeof(saved_shared));
    RtlCopyMemory(tables->wide_shadow,input,A04_VENDOR_WIDE_BYTES);tables->wide_shadow_valid=TRUE;
    if(o->wide_guard_samples<3u)o->wide_guard_samples++;
    o->wide_frame=FALSE;o->shared_gpu_only=FALSE;a04_gpu_dma_barrier();goto out;
poison:
    tables->wide_shadow_valid=FALSE;r->poisoned=TRUE;s->command_poisoned=TRUE;
    if(NT_SUCCESS(status))status=STATUS_IO_DEVICE_ERROR;
out:
    InterlockedExchange(&s->command_busy,0);return status;
}
#endif
