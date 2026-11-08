/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native m1.11 SYNC transport adapted from mouseos/xnu_kernel_benesse_a04br3
 * A04BR3RogueCommand.h, f5d43bf1677955e7e367545b9bbbd6449776cdda. */
#include "windows_command.h"
#include "A04BR3RogueCommand.h"
#include "A04BR3RogueFirmwareRequests.h"
#include "../../windows_hw.h"
static void fence_barrier(void *ctx)
{ UNREFERENCED_PARAMETER(ctx);a04_gpu_dma_barrier(); }
static void fence_kick(void *ctx,uint32_t offset,uint32_t value)
{
    struct a04_vendor_start *s=ctx;
    WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+offset),value);
}
static void fence_wait(void *ctx,uint32_t usec)
{ UNREFERENCED_PARAMETER(ctx);KeStallExecutionProcessor(usec); }
NTSTATUS a04_vendor_m1_11_windows_irq_snapshot(struct a04_vendor_start *s,
    BOOLEAN coherent_or_uncached,struct a04_vendor_irq_snapshot *snapshot)
{
    struct a04_rgx_fw_runtime *r;RGXFWIF_CCB_CTL *ctl;RGXFWIF_TRACEBUF *trace;
    uint32_t count,after,rd,wr;NTSTATUS status=STATUS_DEVICE_BUSY;unsigned i;
    if(!snapshot) return STATUS_INVALID_PARAMETER;
    if(!s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !s->online || !s->attempted ||
       !s->memory || !s->memory->hardware_owned || !s->memory->runtime || !s->core)
        return STATUS_DEVICE_NOT_READY;
    if(coherent_or_uncached!=TRUE) return STATUS_NOT_SUPPORTED;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0) return STATUS_DEVICE_BUSY;
    if(s->command_poisoned) { status=STATUS_IO_DEVICE_ERROR;goto done; }
    r=s->memory->runtime;
    if(!r->init) {status=STATUS_INVALID_DEVICE_STATE;goto done;}
    ctl=(RGXFWIF_CCB_CTL *)a04_rgx_command_memory(r,r->init->psFirmwareCCBCtl.ui32Addr,sizeof(*ctl));
    trace=(RGXFWIF_TRACEBUF *)a04_rgx_command_memory(r,r->init->sTraceBufCtl.ui32Addr,sizeof(*trace));
    if(!ctl || ctl!=r->fctl || !trace || trace!=r->trace) {status=STATUS_INVALID_DEVICE_STATE;goto done;}
    for(i=0;i<4;i++) {
        a04_gpu_dma_barrier();count=trace->aui32InterruptCount[0];
        rd=ctl->ui32ReadOffset;wr=ctl->ui32WriteOffset;
        if(ctl->ui32WrapMask!=31 || ctl->ui32CmdSize!=sizeof(RGXFWIF_FWCCB_CMD) || rd>31 || wr>31) {
            s->command_poisoned=TRUE;status=STATUS_IO_DEVICE_ERROR;goto done;
        }
        if(rd!=wr) {status=STATUS_DEVICE_BUSY;goto done;}
        a04_gpu_dma_barrier();after=trace->aui32InterruptCount[0];
        if(after==count) {snapshot->count=count;snapshot->valid=TRUE;status=STATUS_SUCCESS;break;}
    }
done:
    InterlockedExchange(&s->command_busy,0);return status;
}
NTSTATUS a04_vendor_m1_11_windows_requests(struct a04_vendor_start *s,
    BOOLEAN coherent_or_uncached,a04_vendor_request_handler handler,void *context,
    uint32_t budget,uint32_t *handled,BOOLEAN *more)
{
    struct a04_rgx_request_io io;int result;
    if (!handled || !more || !handler || !budget || budget>31) return STATUS_INVALID_PARAMETER;
    *handled=0;*more=FALSE;
    if (!s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !s->online || !s->attempted ||
        !s->memory || !s->memory->hardware_owned || !s->memory->runtime || !s->core)
        return STATUS_DEVICE_NOT_READY;
    if (coherent_or_uncached!=TRUE) return STATUS_NOT_SUPPORTED;
    if (InterlockedCompareExchange(&s->command_busy,1,0)!=0) return STATUS_DEVICE_BUSY;
    if (s->command_poisoned) { InterlockedExchange(&s->command_busy,0);return STATUS_IO_DEVICE_ERROR; }
    io.context=context;io.acquire=fence_barrier;io.publish=fence_barrier;io.handle=handler;
    result=a04_rgx_firmware_requests(s->memory->runtime,&io,budget,handled);
    if (result==A04_REQUEST_CORRUPT) s->command_poisoned=TRUE;
    if (result==A04_REQUEST_MORE) *more=TRUE;
    InterlockedExchange(&s->command_busy,0);
    if (result==A04_REQUEST_EMPTY || result==A04_REQUEST_MORE) return STATUS_SUCCESS;
    if (result==A04_REQUEST_BLOCKED) return STATUS_NOT_SUPPORTED;
    if (result==A04_REQUEST_INVALID) return STATUS_INVALID_PARAMETER;
    return STATUS_IO_DEVICE_ERROR;
}
NTSTATUS a04_vendor_m1_11_windows_fence(struct a04_vendor_start *s,
    uint32_t dm,BOOLEAN coherent_or_uncached)
{
    struct a04_rgx_command_io io;struct a04_rgx_command_state state;
    int result;NTSTATUS status;
    if(!s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !s->online || !s->attempted ||
       !s->memory || !s->memory->hardware_owned || !s->memory->runtime || !s->core)
        return STATUS_DEVICE_NOT_READY;
    if(coherent_or_uncached!=TRUE) return STATUS_NOT_SUPPORTED;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0) return STATUS_DEVICE_BUSY;
    if(s->shutdown_started) { InterlockedExchange(&s->command_busy,0);return STATUS_IO_DEVICE_ERROR; }
    state.sequence=s->command_sequence;state.poisoned=s->command_poisoned!=FALSE;
    io.context=s;io.publish=fence_barrier;io.acquire=fence_barrier;io.kick=fence_kick;io.wait_us=fence_wait;
    result=a04_rgx_command_fence(s->memory->runtime,&state,&io,dm,10000,10);
    s->command_sequence=state.sequence;s->command_poisoned=state.poisoned?TRUE:FALSE;
    InterlockedExchange(&s->command_busy,0);
    if(result==A04_CCB_OK) status=STATUS_SUCCESS;
    else if(result==A04_CCB_TIMEOUT) status=STATUS_IO_TIMEOUT;
    else if(result==A04_CCB_FULL || result==A04_CCB_BUSY) status=STATUS_DEVICE_BUSY;
    else if(result==A04_CCB_INPUT) status=STATUS_INVALID_PARAMETER;
    else status=STATUS_IO_DEVICE_ERROR;
    return status;
}
NTSTATUS a04_vendor_m1_11_windows_power_off(struct a04_vendor_start *s,
    BOOLEAN coherent_or_uncached)
{
    struct a04_rgx_command_io io;struct a04_rgx_command_state state;int result;
    if(!s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !s->online || !s->attempted ||
       !s->memory || !s->memory->hardware_owned || !s->memory->runtime || !s->core)
        return STATUS_DEVICE_NOT_READY;
    if(coherent_or_uncached!=TRUE) return STATUS_NOT_SUPPORTED;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0) return STATUS_DEVICE_BUSY;
    if(s->shutdown_started) { InterlockedExchange(&s->command_busy,0);return STATUS_IO_DEVICE_ERROR; }
    state.sequence=s->command_sequence;state.poisoned=s->command_poisoned!=FALSE;
    io.context=s;io.publish=fence_barrier;io.acquire=fence_barrier;io.kick=fence_kick;io.wait_us=fence_wait;
    result=a04_rgx_command_power_off(s->memory->runtime,&state,&io,10000,10);
    s->command_sequence=state.sequence;s->command_poisoned=state.poisoned?TRUE:FALSE;
    if(result==A04_CCB_OK) s->power_off_ack=TRUE;
    if(result==A04_CCB_OK || result==A04_CCB_TIMEOUT || result==A04_CCB_CORRUPT)
        s->online=FALSE;
    InterlockedExchange(&s->command_busy,0);
    if(result==A04_CCB_OK) return STATUS_SUCCESS;
    if(result==A04_CCB_TIMEOUT) return STATUS_IO_TIMEOUT;
    if(result==A04_CCB_POWER_DENIED) return STATUS_DEVICE_BUSY;
    if(result==A04_CCB_FULL || result==A04_CCB_BUSY) return STATUS_DEVICE_BUSY;
    if(result==A04_CCB_INPUT) return STATUS_INVALID_PARAMETER;
    return STATUS_IO_DEVICE_ERROR;
}

/* Forced system power transition: no command or memory release after an
 * uncertain acknowledgement. Keep serialization across both firmware steps. */
NTSTATUS a04_vendor_m1_11_windows_forced_power_off(struct a04_vendor_start *s,
    BOOLEAN coherent_or_uncached,NTSTATUS *idle_status)
{
    struct a04_rgx_command_io io;struct a04_rgx_command_state state;int result;
    NTSTATUS status=STATUS_DEVICE_BUSY;
    if(!idle_status)return STATUS_INVALID_PARAMETER;
    *idle_status=STATUS_NOT_SUPPORTED;
    if(!s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !s->online || !s->attempted ||
       !s->memory || !s->memory->hardware_owned || !s->memory->runtime || !s->core || s->shutdown_started)
        return STATUS_DEVICE_NOT_READY;
    if(coherent_or_uncached!=TRUE)return STATUS_NOT_SUPPORTED;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0)return STATUS_DEVICE_BUSY;
    if(s->shutdown_started || s->command_poisoned){status=STATUS_INVALID_DEVICE_STATE;goto out;}
    s->shutdown_started=TRUE;
    state.sequence=s->command_sequence;state.poisoned=FALSE;
    io.context=s;io.publish=fence_barrier;io.acquire=fence_barrier;io.kick=fence_kick;io.wait_us=fence_wait;
    result=a04_rgx_command_power_transition(s->memory->runtime,&state,&io,A04_POWER_FORCE_IDLE,10000,10);
    if(result==A04_CCB_OK) *idle_status=STATUS_SUCCESS;
    else if(result==A04_CCB_TIMEOUT) *idle_status=STATUS_IO_TIMEOUT;
    else if(result==A04_CCB_POWER_DENIED || result==A04_CCB_BUSY || result==A04_CCB_FULL) *idle_status=STATUS_DEVICE_BUSY;
    else if(result==A04_CCB_INPUT) *idle_status=STATUS_INVALID_PARAMETER;
    else *idle_status=STATUS_IO_DEVICE_ERROR;
    status=*idle_status;
    if(result==A04_CCB_OK) {
        result=a04_rgx_command_power_transition(s->memory->runtime,&state,&io,A04_POWER_FORCED_OFF,10000,10);
        if(result==A04_CCB_OK){s->power_off_ack=TRUE;s->online=FALSE;status=STATUS_SUCCESS;}
        else if(result==A04_CCB_TIMEOUT)status=STATUS_IO_TIMEOUT;
        else if(result==A04_CCB_POWER_DENIED || result==A04_CCB_BUSY || result==A04_CCB_FULL)status=STATUS_DEVICE_BUSY;
        else if(result==A04_CCB_INPUT)status=STATUS_INVALID_PARAMETER;
        else status=STATUS_IO_DEVICE_ERROR;
    }
    s->command_sequence=state.sequence;s->command_poisoned=state.poisoned?TRUE:FALSE;
    if(state.poisoned)s->online=FALSE;
out:
    InterlockedExchange(&s->command_busy,0);return status;
}

NTSTATUS a04_vendor_m1_11_power_debug_snapshot(struct a04_vendor_start *s, ULONG values[6])
{
    struct a04_rgx_fw_runtime *r;
    if(!s || !values || !s->memory || !s->memory->hardware_owned ||
       !(r=s->memory->runtime) || !r->power_sync || !r->trace || !r->kctl || !r->fctl)
        return STATUS_INVALID_DEVICE_STATE;
    a04_gpu_dma_barrier();
    values[0]=*(volatile ULONG *)r->power_sync;
    values[1]=(ULONG)((volatile RGXFWIF_TRACEBUF *)r->trace)->ePowState;
    values[2]=((volatile RGXFWIF_CCB_CTL *)r->kctl)->ui32ReadOffset;
    values[3]=((volatile RGXFWIF_CCB_CTL *)r->kctl)->ui32WriteOffset;
    values[4]=((volatile RGXFWIF_CCB_CTL *)r->fctl)->ui32ReadOffset;
    values[5]=((volatile RGXFWIF_CCB_CTL *)r->fctl)->ui32WriteOffset;
    return STATUS_SUCCESS;
}

NTSTATUS a04_vendor_m1_11_fw_debug_snapshot(struct a04_vendor_start *s,struct a04_vendor_fw_debug *out)
{
    struct a04_rgx_fw_runtime *r;RGXFWIF_FWCCB_CMD *ring;RGXFWIF_CCB_CTL ctl;
    unsigned n;
    if(!out)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(out,sizeof(*out));
    if(!s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !s->memory || !s->memory->hardware_owned || !s->memory->uncached ||
       !(r=s->memory->runtime) || !r->init || !r->fctl)return STATUS_INVALID_DEVICE_STATE;
    if(InterlockedCompareExchange(&s->command_busy,1,0)!=0)return STATUS_DEVICE_BUSY;
    a04_gpu_dma_barrier();
    if(a04_rgx_device_read_words(&ctl,r->fctl,sizeof(ctl)) || ctl.ui32WrapMask!=31 ||
       ctl.ui32CmdSize!=sizeof(RGXFWIF_FWCCB_CMD) || ctl.ui32ReadOffset>31 || ctl.ui32WriteOffset>31)goto bad;
    ring=(RGXFWIF_FWCCB_CMD *)a04_rgx_command_memory(r,
        ((volatile RGXFWIF_INIT *)r->init)->psFirmwareCCB.ui32Addr,32u*sizeof(*ring));
    if(!ring)goto bad;
    out->read=ctl.ui32ReadOffset;out->write=ctl.ui32WriteOffset;
    out->count=(out->write-out->read)&31u;if(out->count>2)out->count=2;
    for(n=0;n<out->count;n++)
        if(a04_rgx_device_read_words(out->words[n],&ring[(out->read+n)&31u],sizeof(*ring)))goto bad;
    InterlockedExchange(&s->command_busy,0);return STATUS_SUCCESS;
bad:
    RtlZeroMemory(out,sizeof(*out));InterlockedExchange(&s->command_busy,0);return STATUS_IO_DEVICE_ERROR;
}
