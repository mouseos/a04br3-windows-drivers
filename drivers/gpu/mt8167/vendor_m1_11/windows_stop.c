/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_stop.h"
#include "A04BR3RogueStop.h"
static uint32_t stop_read32(void *ctx,uint32_t off)
{
    struct a04_vendor_start *s=ctx;
    return READ_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+off));
}
static uint64_t stop_read64(void *ctx,uint32_t off)
{
    uint32_t hi=stop_read32(ctx,off+4),lo=stop_read32(ctx,off);
    return ((uint64_t)hi<<32)|lo;
}
static void stop_write32(void *ctx,uint32_t off,uint32_t value)
{
    struct a04_vendor_start *s=ctx;
    WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(s->core+off),value);
}
static void stop_wait(void *ctx,uint32_t usec)
{ UNREFERENCED_PARAMETER(ctx);KeStallExecutionProcessor(usec); }
NTSTATUS a04_vendor_m1_11_windows_stop(struct a04_vendor_start *s,
    BOOLEAN interrupts_drained,ULONG *failed_register,uint64_t *last_value)
{
    struct a04_rgx_exec_io io={0};struct a04_rgx_stop_failure failure={0};int result;
    if(failed_register)*failed_register=0;
    if(last_value)*last_value=0;
    if(!s || !failed_register || !last_value || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
       !s->attempted || s->online || !s->power_off_ack || !s->memory ||
       !s->memory->hardware_owned || !s->core || interrupts_drained!=TRUE)
        return STATUS_INVALID_DEVICE_STATE;
    io.context=s;io.read32=stop_read32;io.read64=stop_read64;io.write32=stop_write32;io.wait_us=stop_wait;
    result=a04_rgx_ddk_stop(&io,0x001600280036001eULL,1,1,10000,100,&failure);
    *failed_register=failure.offset;*last_value=failure.value;
    return result==A04_EXEC_OK?STATUS_SUCCESS:STATUS_IO_TIMEOUT;
}
