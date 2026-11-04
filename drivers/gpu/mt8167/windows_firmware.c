/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Windows-side firmware boot composition. This is not a PnP or WDDM driver. */
#include "windows_firmware.h"
#include "pvr_rogue_fwif.h"
#define BOOT_TIMEOUT_STEPS 1000u
#define TRACE(s,n) do { if ((s)->trace) (s)->trace((s)->trace_context,(n)); } while (0)
static NTSTATUS fw_status(enum a04_pvr_fw_result r)
{
    return r==A04_PVR_FW_INCOMPATIBLE ? STATUS_NOT_SUPPORTED : STATUS_INVALID_IMAGE_FORMAT;
}
NTSTATUS a04_gpu_firmware_boot(struct a04_gpu_firmware_state *s,
        PDMA_ADAPTER adapter, volatile void *regs, ULONG regs_length,
        const uint8_t *image, size_t image_size, uint64_t bvnc,
        BOOLEAN allow_mismatch, ULONG clock_hz)
{
    NTSTATUS status; enum a04_pvr_fw_result r; unsigned i; LARGE_INTEGER interval;
    volatile struct rogue_fwif_sysinit *sys;
    if (!s || !adapter || !image || !clock_hz || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
        s->memory.adapter || s->processor_released) return STATUS_INVALID_PARAMETER;
    if (bvnc!=A04_PVR_BVNC) return STATUS_NOT_SUPPORTED;
    TRACE(s,1);
    r=a04_pvr_fw_plan(image,image_size,bvnc,allow_mismatch,&s->plan);
    if (r!=A04_PVR_FW_OK) return fw_status(r);
    status=a04_gpu_register_io_init(&s->registers,regs,regs_length,&s->io);
    if (!NT_SUCCESS(status)) return status;
    status=a04_gpu_allocate_memory(adapter,&s->memory);
    if (!NT_SUCCESS(status)) return status;
    TRACE(s,2);
    r=a04_pvr_fw_load(image,image_size,bvnc,allow_mismatch,s->memory.heap,
                      A04_PVR_HEAP_SIZE,&s->plan);
    if (r!=A04_PVR_FW_OK) { status=fw_status(r); goto fail_before_release; }
    TRACE(s,3);
    r=a04_pvr_mips_prepare(&s->plan,s->memory.heap,A04_PVR_HEAP_SIZE,
        (uint64_t)s->memory.heap_logical.QuadPart,s->memory.table,0x4000u,
        (uint64_t)s->memory.table_logical.QuadPart,&s->remap);
    if (r!=A04_PVR_FW_OK) { status=STATUS_DEVICE_CONFIGURATION_ERROR; goto fail_before_release; }
    TRACE(s,4);
    r=a04_pvr_runtime_prepare(&s->plan,s->memory.heap,A04_PVR_HEAP_SIZE,
        (uint64_t)s->memory.heap_logical.QuadPart,clock_hz,&s->runtime);
    if (r!=A04_PVR_FW_OK) { status=STATUS_DEVICE_CONFIGURATION_ERROR; goto fail_before_release; }
    TRACE(s,5);
    status=a04_gpu_dma_sync(s->memory.heap,A04_PVR_HEAP_SIZE,FALSE);
    if (!NT_SUCCESS(status)) goto fail_before_release;
    TRACE(s,6);
    status=a04_gpu_dma_sync(s->memory.table,0x4000u,FALSE);
    if (!NT_SUCCESS(status)) goto fail_before_release;
    TRACE(s,7);
    /* Mark the allocation as owned by the GPU before the first MMIO write. */
    s->processor_released=TRUE;
    r=a04_pvr_mips_start(&s->io,bvnc,&s->remap);
    if (r!=A04_PVR_FW_OK) return STATUS_DEVICE_CONFIGURATION_ERROR;
    TRACE(s,8);
    sys=(volatile struct rogue_fwif_sysinit *)(void *)((uint8_t *)s->memory.heap+s->runtime.system_init);
    interval.QuadPart=-100000; /* 10 ms, relative */
    for (i=0;i<BOOT_TIMEOUT_STEPS;++i) {
        status=a04_gpu_dma_sync((void *)sys,sizeof(*sys),TRUE);
        if (!NT_SUCCESS(status)) return status;
        if (sys->firmware_started) { s->firmware_started=TRUE; TRACE(s,9); return STATUS_SUCCESS; }
        status=KeDelayExecutionThread(KernelMode,FALSE,&interval);
        if (!NT_SUCCESS(status)) return status;
    }
    return STATUS_IO_TIMEOUT;
fail_before_release:
    a04_gpu_free_memory(&s->memory);
    return status;
}
NTSTATUS a04_gpu_firmware_stop(struct a04_gpu_firmware_state *s,BOOLEAN acknowledged)
{
    enum a04_pvr_fw_result r; struct a04_pvr_stop_failure failure={0};
    if (!s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !s->memory.adapter || !s->processor_released)
        return STATUS_INVALID_PARAMETER;
    if (!acknowledged) return STATUS_DEVICE_BUSY;
    r=a04_pvr_mips_stop_report(&s->io,A04_PVR_BVNC,&failure);
    if (r!=A04_PVR_FW_OK) {
        TRACE(s,0x20000000u|failure.reg);
        TRACE(s,(uint32_t)failure.value);
        TRACE(s,(uint32_t)(failure.value>>32));
        return STATUS_IO_TIMEOUT;
    }
    a04_gpu_dma_barrier();
    a04_gpu_free_memory(&s->memory);
    s->processor_released=FALSE; s->firmware_started=FALSE; s->queue_faulted=FALSE;
    return STATUS_SUCCESS;
}

/* The single-queue operations below require the caller's device lock. */
#include "pvr_rogue_defs.h"
#define A04_GPU_CONFIG_OFFSET (A04_PVR_HEAP_SIZE-3u*65536u)
#define A04_GPU_KCCB_COUNT 128u
static int heap_range(uint32_t off,size_t size)
{ return off<=A04_PVR_HEAP_SIZE && size<=A04_PVR_HEAP_SIZE-off; }
static NTSTATUS send_command(struct a04_gpu_firmware_state *s,unsigned kind)
{
    uint8_t *heap; volatile struct rogue_fwif_ccb_ctl *ctl;
    struct rogue_fwif_kccb_cmd *commands, *cmd;
    volatile uint32_t *returns,*power,*mmu; struct rogue_fwif_connection_ctl *conn;
    volatile struct rogue_fwif_osdata *os;
    uint32_t slot,next,value,executed_before,token=0; LARGE_INTEGER interval; unsigned i; NTSTATUS status;
    if (!s || KeGetCurrentIrql()!=PASSIVE_LEVEL || !s->processor_released || !s->firmware_started ||
        !s->memory.heap || !s->io.write32 || kind>3) return STATUS_INVALID_PARAMETER;
    if (s->queue_faulted) return STATUS_INVALID_DEVICE_STATE;
    if (!heap_range(s->runtime.kernel_ccb_control,sizeof(*ctl)) ||
        !heap_range(s->runtime.kernel_ccb,A04_GPU_KCCB_COUNT*sizeof(*cmd)) ||
        !heap_range(s->runtime.kernel_return_slots,A04_GPU_KCCB_COUNT*sizeof(*returns)) ||
        !heap_range(s->runtime.power_sync,sizeof(*power)) ||
        !heap_range(s->runtime.os_data,sizeof(*os)) ||
        !heap_range(s->runtime.mmu_sync,sizeof(*mmu))) return STATUS_DEVICE_CONFIGURATION_ERROR;
    heap=s->memory.heap;
    ctl=(volatile struct rogue_fwif_ccb_ctl *)(void *)(heap+s->runtime.kernel_ccb_control);
    commands=(struct rogue_fwif_kccb_cmd *)(void *)(heap+s->runtime.kernel_ccb);
    returns=(volatile uint32_t *)(void *)(heap+s->runtime.kernel_return_slots);
    power=(volatile uint32_t *)(void *)(heap+s->runtime.power_sync);
    mmu=(volatile uint32_t *)(void *)(heap+s->runtime.mmu_sync);
    conn=(struct rogue_fwif_connection_ctl *)(void *)(heap+A04_GPU_CONFIG_OFFSET);
    os=(volatile struct rogue_fwif_osdata *)(void *)(heap+s->runtime.os_data);
    TRACE(s,0x20u+kind*0x10u);
    status=a04_gpu_dma_sync((void *)ctl,sizeof(*ctl),TRUE);
    if (!NT_SUCCESS(status)) return status;
    slot=ctl->write_offset;
    if (ctl->wrap_mask!=A04_GPU_KCCB_COUNT-1u || slot>=A04_GPU_KCCB_COUNT ||
        ctl->read_offset>=A04_GPU_KCCB_COUNT) return STATUS_DEVICE_CONFIGURATION_ERROR;
    next=(slot+1u)&(A04_GPU_KCCB_COUNT-1u);
    if (next==ctl->read_offset) return STATUS_DEVICE_BUSY;
    status=a04_gpu_dma_sync((void *)os,sizeof(*os),TRUE);
    if (!NT_SUCCESS(status)) return status;
    executed_before=os->kccb_cmds_executed;
    if (kind==3) {
        status=a04_gpu_dma_sync((void *)mmu,sizeof(*mmu),TRUE);
        if (!NT_SUCCESS(status)) return status;
        token=*mmu+1u;
        if (!token) token=1u;
    }
    cmd=&commands[slot]; RtlZeroMemory(cmd,sizeof(*cmd)); returns[slot]=ROGUE_FWIF_KCCB_RTN_SLOT_NO_RESPONSE;
    if (kind==0) cmd->cmd_type=ROGUE_FWIF_KCCB_CMD_HEALTH_CHECK;
    else if (kind==3) {
        cmd->cmd_type=ROGUE_FWIF_KCCB_CMD_MMUCACHE;
        cmd->cmd_data.mmu_cache_data.cache_flags=ROGUE_FWIF_MMUCACHEDATA_FLAGS_PT |
            ROGUE_FWIF_MMUCACHEDATA_FLAGS_PD | ROGUE_FWIF_MMUCACHEDATA_FLAGS_PC |
            ROGUE_FWIF_MMUCACHEDATA_FLAGS_TLB | ROGUE_FWIF_MMUCACHEDATA_FLAGS_CTX_ALL;
        cmd->cmd_data.mmu_cache_data.mmu_cache_sync_fw_addr=0xc0000000u|s->runtime.mmu_sync;
        cmd->cmd_data.mmu_cache_data.mmu_cache_sync_update_value=token;
    } else {
        *power=0; cmd->cmd_type=ROGUE_FWIF_KCCB_CMD_POW;
        if (kind==1) {
            cmd->cmd_data.pow_data.pow_type=ROGUE_FWIF_POW_FORCED_IDLE_REQ;
            cmd->cmd_data.pow_data.power_req_data.pow_request_type=ROGUE_FWIF_POWER_FORCE_IDLE;
        } else {
            cmd->cmd_data.pow_data.pow_type=ROGUE_FWIF_POW_OFF_REQ;
            cmd->cmd_data.pow_data.power_req_data.forced=TRUE;
        }
    }
    conn->connection_os_state=ROGUE_FW_CONNECTION_OS_ACTIVE;
    TRACE(s,0x21u+kind*0x10u);
    status=a04_gpu_dma_sync(cmd,sizeof(*cmd),FALSE);
    if (NT_SUCCESS(status)) status=a04_gpu_dma_sync((void *)&returns[slot],sizeof(*returns),FALSE);
    if (NT_SUCCESS(status) && (kind==1 || kind==2)) status=a04_gpu_dma_sync((void *)power,sizeof(*power),FALSE);
    if (NT_SUCCESS(status)) status=a04_gpu_dma_sync(conn,sizeof(*conn),FALSE);
    if (!NT_SUCCESS(status)) return status;
    ctl->write_offset=next;
    TRACE(s,0x22u+kind*0x10u);
    status=a04_gpu_dma_sync((void *)ctl,sizeof(*ctl),FALSE);
    if (!NT_SUCCESS(status)) { s->queue_faulted=TRUE; return status; }
    /* MT8167 vendor DDK uses a counted GP kick for this MIPS firmware. */
    s->io.write32(s->io.context,ROGUE_CR_MTS_SCHEDULE,
        (PVR_FWIF_DM_GP & ~ROGUE_CR_MTS_SCHEDULE_DM_CLRMSK)|ROGUE_CR_MTS_SCHEDULE_TASK_COUNTED);
    TRACE(s,0x23u+kind*0x10u);
    interval.QuadPart=-100000;
    for (i=0;i<100u;++i) {
        status=(kind==0 || kind==3) ? a04_gpu_dma_sync((void *)&returns[slot],sizeof(*returns),TRUE)
                                    : a04_gpu_dma_sync((void *)power,sizeof(*power),TRUE);
        if (!NT_SUCCESS(status)) { s->queue_faulted=TRUE; return status; }
        value=(kind==0 || kind==3) ? returns[slot] : *power;
        if ((kind==0 || kind==3) && (value&ROGUE_FWIF_KCCB_RTN_SLOT_POLL_FAILURE)) {
            s->queue_faulted=TRUE; return STATUS_DEVICE_CONFIGURATION_ERROR;
        }
        if ((kind==0 && (value&ROGUE_FWIF_KCCB_RTN_SLOT_CMD_EXECUTED)) || ((kind==1 || kind==2) && value!=0)) return STATUS_SUCCESS;
        /* This core's firmware advances the executed counter but may leave
         * the return slot at zero. Only use the counter for the health check;
         * power transitions still require their explicit acknowledgement. */
        if (kind==0 || kind==3) {
            status=a04_gpu_dma_sync((void *)os,sizeof(*os),TRUE);
            if (!NT_SUCCESS(status)) { s->queue_faulted=TRUE; return status; }
            if (kind==0 && os->kccb_cmds_executed!=executed_before) return STATUS_SUCCESS;
            if (kind==3) {
                status=a04_gpu_dma_sync((void *)mmu,sizeof(*mmu),TRUE);
                if (!NT_SUCCESS(status)) { s->queue_faulted=TRUE; return status; }
                if (os->kccb_cmds_executed!=executed_before && *mmu==token) return STATUS_SUCCESS;
            }
        }
        status=KeDelayExecutionThread(KernelMode,FALSE,&interval);
        if (!NT_SUCCESS(status)) { s->queue_faulted=TRUE; return status; }
    }
    s->queue_faulted=TRUE; return STATUS_IO_TIMEOUT;
}
NTSTATUS a04_gpu_firmware_health(struct a04_gpu_firmware_state *s) { return send_command(s,0); }
NTSTATUS a04_gpu_firmware_invalidate_mmu(struct a04_gpu_firmware_state *s) { return send_command(s,3); }
NTSTATUS a04_gpu_firmware_shutdown(struct a04_gpu_firmware_state *s)
{
    NTSTATUS status=send_command(s,1);
    if (!NT_SUCCESS(status)) return status;
    TRACE(s,0x50);
    status=send_command(s,2);
    if (!NT_SUCCESS(status)) return status;
    TRACE(s,0x51);
    return a04_gpu_firmware_stop(s,TRUE);
}
