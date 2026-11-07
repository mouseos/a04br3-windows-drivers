/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Owned-GPU operational retirement follows normal m1.11 PVRSRVDeviceDestroy
 * -> forced idle -> device pre-power/RGXStop -> MTK platform off -> ISR/MISR
 * rundown -> FW free. Never follow force-unload/bad-services bypasses. */
#include "native_owner_factory_private.h"
#include "windows_native_cold_private.h"
#include "../windows_hw.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
static BOOLEAN gate_closed(void *p)
{struct e_native_owner *c=p;InterlockedExchange(&c->irq_gate,0);return TRUE;}
struct irq_call {struct e_native_owner *c;NTSTATUS status;};
static BOOLEAN drain_begin(void *p)
{struct irq_call *x=p;x->status=a04_native_irq_begin_off_drain(&x->c->allocation->irq,x->c->hardware_epoch);return TRUE;}
static BOOLEAN drain_close(void *p)
{
    struct irq_call *x=p;struct e_native_owner *c=x->c;
    x->status=a04_native_irq_close_off(&c->allocation->irq,c->hardware_epoch,&c->off_irq);
    if(NT_SUCCESS(x->status))InterlockedExchange(&c->irq_gate,0);
    return TRUE;
}
static NTSTATUS irq_phase(struct e_native_owner *c,PKSYNCHRONIZE_ROUTINE routine)
{
    struct irq_call call={c,STATUS_UNSUCCESSFUL};
    for(unsigned i=0;i<20000;i++){
        if(!e_native_synchronize(c,routine,&call))return STATUS_IO_DEVICE_ERROR;
        if(NT_SUCCESS(call.status))return call.status;
        if(call.status!=STATUS_DEVICE_BUSY &&
           !(routine==drain_close && call.status==STATUS_INVALID_DEVICE_STATE &&
             c->allocation->irq.draining && !c->allocation->irq.sampled_fw_valid && !c->allocation->irq.faulted))return call.status;
        KeStallExecutionProcessor(50);
    }
    return STATUS_IO_TIMEOUT;
}
NTSTATUS e_native_stop_locked(struct e_native_owner *c)
{
    struct a04_native_owner_allocation *a=c->allocation;NTSTATUS status,idle;unsigned failed;
    if(!a || !c->platform_held)return STATUS_INVALID_DEVICE_STATE;
    if(c->receipt_valid){
        if(c->hardware_off)return c->platform.VerifyOff(c->platform.Header.Context,c->platform_token);
        if(!a->firmware.hardware_owned && !a->start.attempted)return STATUS_SUCCESS;
        return STATUS_INVALID_DEVICE_STATE;
    }
    if(!a->firmware.hardware_owned && !a->start.attempted){
        /* The staged page-table root has never been published. This is the
         * same cold-init failure/unstarted-device class as DDK init cleanup,
         * not a fabricated FW ack or an old transaction drain observation. */
        if(!e_native_synchronize(c,gate_closed,c))return STATUS_IO_DEVICE_ERROR;
        c->receipt_valid=TRUE;c->job_state=E_JOB_NONE;return STATUS_SUCCESS;
    }
    if(!a->start.online || a->start.command_poisoned || a->render.poisoned)return STATUS_DEVICE_BUSY;
    if(c->job_state==E_JOB_SUBMITTED && a->dispatch.state==A04_NATIVE_DISPATCH_SUBMITTED){
        status=a04_vendor_native_owner_finish_passive(a->native,&a->start,&a->dispatch.result);
        if(!NT_SUCCESS(status))return status;
        /* Stop retires this token as aborted, it never fabricates a scheduler
         * completion from the passive finish/cache observation. */
    }
    status=irq_phase(c,drain_begin);if(!NT_SUCCESS(status))return status;
    status=a04_vendor_render_cleanup_contexts(&a->render,&a->start,&failed);if(!NT_SUCCESS(status))return status;
    if(a->render.cleanup_mask!=15)return STATUS_IO_DEVICE_ERROR;
    status=a04_vendor_m1_11_windows_forced_power_off(&a->start,TRUE,&idle);
    if(!NT_SUCCESS(status) || !NT_SUCCESS(idle) || !a->start.power_off_ack || a->start.online)return NT_SUCCESS(status)?STATUS_IO_DEVICE_ERROR:status;
    status=irq_phase(c,drain_close);if(!NT_SUCCESS(status))return status;
    status=a04_vendor_m1_11_windows_core_hold_reset(&a->start,TRUE,&c->reset);if(!NT_SUCCESS(status))return status;
    if(!c->reset.reset_held || !c->off_irq.source_closed || c->off_irq.epoch!=c->hardware_epoch)return STATUS_IO_DEVICE_ERROR;
    /* No further GPU0/MFG read after domain OFF. The legitimate parent owns
     * only SPM/INFRA and independently re-reads the actual OFF conditions. */
    WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(c->mfg+4),15u);
    status=c->platform.Off(c->platform.Header.Context,c->platform_token);if(!NT_SUCCESS(status))return status;
    c->hardware_off=TRUE;
    status=c->platform.VerifyOff(c->platform.Header.Context,c->platform_token);if(!NT_SUCCESS(status))return status;
    if(a->binding){status=a04_native_mdl_sync_for_cpu(a->binding);if(!NT_SUCCESS(status))return status;}
    c->receipt_valid=TRUE;c->job_state=E_JOB_NONE;
    return STATUS_SUCCESS;
}
NTSTATUS e_native_retire_graph_locked(struct e_native_owner *c)
{
    struct a04_native_owner_allocation *a=c->allocation;NTSTATUS status;
    if(!a)return STATUS_SUCCESS;
    if(a->firmware.hardware_owned || a->start.attempted){
        if(!c->receipt_valid || !c->hardware_off || c->irq_gate || !a->irq.closed || a->irq.powered ||
           !c->off_irq.source_closed || !c->reset.reset_held || c->off_irq.epoch!=c->hardware_epoch)
            return STATUS_DEVICE_BUSY;
        status=c->platform.VerifyOff(c->platform.Header.Context,c->platform_token);if(!NT_SUCCESS(status))return status;
        /* This marker transition is reachable only through the complete
         * normal owned-GPU retirement path above, never from a user flag. */
        a->firmware.hardware_owned=FALSE;
        if(a->binding){status=a04_native_mdl_retire_after_gpu_off(a->binding);if(!NT_SUCCESS(status))return status;}
        a->start.online=FALSE;
        status=a04_vendor_m1_11_start_release_cold(&a->start);if(!NT_SUCCESS(status))return status;
        /* Actual consumer gate was closed under Dxgk IRQ synchronization;
         * passive accesses to this graph are serialized by the owner mutex.
         * Dxgk retains the OS IRQ connection. Do not claim IoDisconnectInterrupt. */
        a->irq.core=NULL;a->irq.runtime=NULL;a->irq.bytes=0;
        RtlZeroMemory(&a->dispatch,sizeof(a->dispatch));
    }
    if(a->native){status=a04_vendor_native_release_cold(&a->native);if(!NT_SUCCESS(status))return status;}
    if(a->render.firmware){status=a04_vendor_render_release_cold(&a->render);if(!NT_SUCCESS(status))return status;}
    status=a04_vendor_m1_11_windows_free(&a->firmware);if(!NT_SUCCESS(status))return status;
    return STATUS_SUCCESS;
}
NTSTATUS e_native_build_graph_locked(struct e_native_owner *c)
{
    struct a04_native_owner_allocation *a=c->allocation;NTSTATUS status;
    if(a->native)return STATUS_SUCCESS;
    status=a04_vendor_m1_11_windows_stage_uncached(&a->firmware,a->adapter,c->firmware_image,c->firmware_bytes);
    if(!NT_SUCCESS(status))return status;
    status=a04_vendor_render_prepare_cold(&a->render,&a->firmware,a->adapter,FALSE,0);if(!NT_SUCCESS(status))return status;
    status=a04_vendor_native_allocate_cold_ex(&a->native,&a->render,a->adapter,a->map_registers,800,600,&a->diagnostic);
    if(!NT_SUCCESS(status))return status;
    if(a->binding)return a04_vendor_native_attach_mdl_target_preserve_cold(a->native,a->binding,c->aperture_offset,c->colour);
    return STATUS_SUCCESS;
}
static BOOLEAN boot_enable(void *p)
{
    struct irq_call *x=p;struct e_native_owner *c=x->c;
    x->status=a04_native_irq_rearm(&c->allocation->irq,c->hardware_epoch,0);
    if(NT_SUCCESS(x->status))InterlockedExchange(&c->irq_gate,1);
    return TRUE;
}
NTSTATUS e_native_boot_locked(struct e_native_owner *c)
{
    struct a04_native_owner_allocation *a=c->allocation;struct a04_gpu_domain_snapshot p;
    struct a04_vendor_power_snapshot power;struct irq_call call={c,STATUS_UNSUCCESSFUL};NTSTATUS status;
    if(!a || !a->binding || !a->native || c->hardware_epoch==UINT64_MAX)return STATUS_INVALID_DEVICE_STATE;
    status=e_native_platform_on(c);if(!NT_SUCCESS(status))return status;
    status=e_native_platform_observe(c,&p);if(!NT_SUCCESS(status))return status;
    /* MFG range belongs exclusively to this GPU0 miniport, not the parent. */
    WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(c->mfg+8),15u);
    power=(struct a04_vendor_power_snapshot){p.spm_status,p.spm_status2,
        READ_REGISTER_ULONG((volatile ULONG *)(void *)c->mfg),p.mux0,p.mux1,p.slow};
    if(!a04_vendor_m1_11_power_valid(&power))return STATUS_DEVICE_NOT_READY;
    uint64_t bvnc=(uint64_t)READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x24))<<32 |
        READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x20));
    status=a04_vendor_m1_11_windows_start(&a->start,&a->firmware,c->core,0x80000,bvnc,&power,A04_VENDOR_IRQ_MASKED);
    if(!NT_SUCCESS(status))return status;
    status=a04_vendor_m1_11_windows_fence(&a->start,0,TRUE);if(!NT_SUCCESS(status))return status;
    c->hardware_epoch++;
    RtlZeroMemory(&a->irq,sizeof(a->irq));
    status=a04_native_irq_bind(&a->irq,c->core,0x80000,a->firmware.runtime,c->hardware_epoch);if(!NT_SUCCESS(status))return status;
    RtlZeroMemory(&a->dispatch,sizeof(a->dispatch));
    status=a04_native_dispatch_initialize(&a->dispatch,a->native,&a->start,&a->irq,c->hardware_epoch,c,e_native_synchronize);
    if(!NT_SUCCESS(status))return status;
    if(!e_native_synchronize(c,boot_enable,&call))return STATUS_IO_DEVICE_ERROR;
    return call.status;
}
