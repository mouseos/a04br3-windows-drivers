/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native m1.11 SYNC transport adapted from mouseos/xnu_kernel_benesse_a04br3
 * A04BR3RogueCommand.h, f5d43bf1677955e7e367545b9bbbd6449776cdda. */
#include "windows_command.h"
#include "A04BR3RogueCommand.h"
#include "A04BR3RogueFirmwareRequests.h"
#include "../windows_hw.h"
static void fence_barrier(void *ctx)
{ UNREFERENCED_PARAMETER(ctx);a04_gpu_dma_barrier(); }
static void fence_kick(void *ctx,uint32_t offset,uint32_t value)
{
    struct a04_vendor_start *s=ctx;
    WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+offset),value);
}
static void fence_wait(void *ctx,uint32_t usec)
{ UNREFERENCED_PARAMETER(ctx);KeStallExecutionProcessor(usec); }
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
    if(result==A04_CCB_FULL || result==A04_CCB_BUSY) return STATUS_DEVICE_BUSY;
    if(result==A04_CCB_INPUT) return STATUS_INVALID_PARAMETER;
    return STATUS_IO_DEVICE_ERROR;
}
