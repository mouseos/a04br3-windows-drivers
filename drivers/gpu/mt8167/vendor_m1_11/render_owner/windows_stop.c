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

static void stop_write64(void *ctx,uint32_t off,uint64_t value)
{
    /* Same scalar low/high ordering as the existing ARM32 startup transport.
     * Firmware/MIPS and engines must already have passed RGXStop idle checks. */
    stop_write32(ctx,off,(uint32_t)value);stop_write32(ctx,off+4,(uint32_t)(value>>32));
}
NTSTATUS a04_vendor_m1_11_windows_core_hold_reset(struct a04_vendor_start *s,
    BOOLEAN drained,struct a04_rgx_core_reset_observation *out)
{
    struct a04_rgx_exec_io io={0};int e;
    if(!out)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(out,sizeof(*out));
    if(!s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !s->attempted || s->online ||
       !s->shutdown_started || !s->power_off_ack || s->command_busy || s->command_poisoned ||
       !s->core || !s->memory || !s->memory->hardware_owned || !s->memory->uncached || drained!=TRUE)
        return STATUS_INVALID_DEVICE_STATE;
    io.context=s;io.read32=stop_read32;io.read64=stop_read64;
    io.write32=stop_write32;io.write64=stop_write64;io.wait_us=stop_wait;
    e=a04_rgx_core_stop_hold_reset(&io,UINT64_C(0x001600280036001e),1,1,10000,10,out);
    if(e==A04_EXEC_OK)return STATUS_SUCCESS;
    s->command_poisoned=TRUE; /* uncertain state: retain every exposed resource */
    return e==A04_EXEC_TIMEOUT?STATUS_IO_TIMEOUT:STATUS_IO_DEVICE_ERROR;
}
