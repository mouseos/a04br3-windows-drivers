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
    if(at>4095u || (at&7u) || at+sizeof(packet)+40u>4096u ||
       a04_rgx_device_read_words(&client,bank+r->layout.control[3],sizeof(client)) ||
       client.ui32WrapMask!=4095u || client.ui32ReadOffset!=at || client.ui32WriteOffset!=at ||
       client.ui32DepOffset>4095u || (client.ui32DepOffset&7u)) {status=STATUS_INVALID_DEVICE_STATE;goto done;}
    seq=++r->sequence;packet.task.eCmdType=TQ_TASK;packet.task.ui32CmdSize=208u;
    RtlCopyMemory(packet.payload,copy?copy->frozen_task:r->clear.payload,sizeof(packet.payload));
    packet.update.eCmdType=CLIENT_UPDATE;packet.update.ui32CmdSize=8;
    packet.ufo.puiAddrUFO.ui32Addr=0xc0080000u+r->layout.sync;packet.ufo.ui32Value=seq;
    *sync=0;if(a04_rgx_device_write_words(bank+r->layout.commands[3]+at,&packet,sizeof(packet))) {status=STATUS_IO_DEVICE_ERROR;goto poison;}
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
        for(i=0;i<1024u;i++) {
            uint32_t expected=a04_vendor_copy_pattern(i);
            value=dst[i];if(value==expected)result->zeros++;
            if(src[i]!=expected)result->guard_errors++;
            if(shared[i]!=(i<9u?copy->state.shared[i]:0xa5a5a5a5u))result->guard_errors++;
            if(i==0)result->first=value;
            if(i==1023u)result->last=value;
        }
        /* The general-heap pages immediately preceding source belong to
         * the cold clear scratch; verify their original guard as well. */
        for(i=0;i<16384u;i++)if(output[i]!=0xa5a5a5a5u)result->guard_errors++;
    } else for(i=0;i<(r->screen_surface ? r->screen_surface->bytes/4u : 16384u);i++) {
        value=output[i];if(a04_vendor_clear_is_target(i*4u,r->clear_first,r->clear_row_bytes)) {if(value==r->clear.expected)result->zeros++;} else if(value!=0xa5a5a5a5u)result->guard_errors++;
        if(i*4u==r->clear_first)result->first=value;
        if(i*4u==r->clear_first+31u*r->clear_row_bytes+124u)result->last=value;
    }
    status=(result->zeros==1024u && !result->guard_errors)?STATUS_SUCCESS:STATUS_DATA_ERROR;
    r->clear_verified=NT_SUCCESS(status);
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
