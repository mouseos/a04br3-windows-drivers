/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Source-linked Dxgk owner. No WDF, second IRQ connection or user payload. */
#include "native_owner_factory_private.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
static void reference(void *p){struct e_native_owner *c=p;InterlockedIncrement(&c->references);}
static void dereference(void *p){struct e_native_owner *c=p;InterlockedDecrement(&c->references);}
static NTSTATUS enter(struct e_native_owner *c,BOOLEAN cleanup)
{
    if(!c || KeGetCurrentIrql()!=PASSIVE_LEVEL || !ExAcquireRundownProtection(&c->operations))return STATUS_DEVICE_BUSY;
    KeWaitForSingleObject(&c->mutex,Executive,KernelMode,FALSE,NULL);
    if(c->closing || !c->leased || (!cleanup && (c->state!=E_OWNER_ACTIVE || c->power!=PowerDeviceD0))){
        KeReleaseMutex(&c->mutex,FALSE);ExReleaseRundownProtection(&c->operations);return STATUS_INVALID_DEVICE_STATE;
    }
    return STATUS_SUCCESS;
}
static NTSTATUS leave(struct e_native_owner *c,NTSTATUS status)
{KeReleaseMutex(&c->mutex,FALSE);ExReleaseRundownProtection(&c->operations);return status;}
static BOOLEAN close_admission(void *p)
{struct e_native_owner *c=p;InterlockedExchange(&c->closing,1);return TRUE;}
static BOOLEAN reopen_admission(void *p)
{struct e_native_owner *c=p;InterlockedExchange(&c->closing,0);return TRUE;}
static NTSTATUS transition_begin(struct e_native_owner *c,uint64_t epoch)
{
    if(!c || KeGetCurrentIrql()!=PASSIVE_LEVEL || !c->leased || c->epoch!=epoch)return STATUS_INVALID_DEVICE_STATE;
    /* Close before waiting: a passive operation may hold rundown while it
     * waits for mutex. Waiting with mutex held would deadlock that operation. */
    if(!e_native_synchronize(c,close_admission,c))return STATUS_IO_DEVICE_ERROR;
    if(!c->rundown_closed){ExWaitForRundownProtectionRelease(&c->operations);c->rundown_closed=TRUE;}
    KeWaitForSingleObject(&c->mutex,Executive,KernelMode,FALSE,NULL);
    return STATUS_SUCCESS;
}
static void proof(struct e_native_owner *c,struct geo_owner_proof *p)
{RtlZeroMemory(p,sizeof(*p));p->size=sizeof(*p);p->version=2;p->epoch=c->epoch;p->flags=GEO_PROOF_REQUIRED;}
static NTSTATUS quiesce(void *p,uint64_t epoch,uint32_t reason,struct geo_owner_proof *out)
{
    struct e_native_owner *c=p;NTSTATUS s;
    if(!out || (reason!=GEO_QUIESCE_STOP && reason!=GEO_QUIESCE_POWER && reason!=GEO_QUIESCE_TDR))return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(out,sizeof(*out));s=transition_begin(c,epoch);if(!NT_SUCCESS(s))return s;
    s=e_native_stop_locked(c);
    if(NT_SUCCESS(s)){s=e_native_retire_graph_locked(c);if(NT_SUCCESS(s)){c->state=E_OWNER_QUIESCED;proof(c,out);
        /* Keep cleanup operations available on retained surfaces after the
         * closed/rundown retirement. GPU admission still requires ACTIVE. */
        ExReInitializeRundownProtection(&c->operations);c->rundown_closed=FALSE;
        if(!e_native_synchronize(c,reopen_admission,c))s=STATUS_IO_DEVICE_ERROR;}}
    if(!NT_SUCCESS(s))c->state=E_OWNER_FAILED;
    KeReleaseMutex(&c->mutex,FALSE);return s;
}
static NTSTATUS reset(void *p,uint64_t epoch,struct geo_owner_proof *out)
{
    struct e_native_owner *c=p;NTSTATUS s;
    if(!c || !out || epoch==UINT64_MAX)return STATUS_INVALID_PARAMETER;
    s=quiesce(p,epoch,GEO_QUIESCE_TDR,out);if(!NT_SUCCESS(s))return s;
    KeWaitForSingleObject(&c->mutex,Executive,KernelMode,FALSE,NULL);
    c->epoch++;proof(c,out);KeReleaseMutex(&c->mutex,FALSE);return STATUS_SUCCESS;
}
static NTSTATUS restart(void *p,uint64_t epoch)
{
    struct e_native_owner *c=p;NTSTATUS s=STATUS_SUCCESS;
    if(!c || KeGetCurrentIrql()!=PASSIVE_LEVEL || !c->leased)return STATUS_INVALID_DEVICE_STATE;
    KeWaitForSingleObject(&c->mutex,Executive,KernelMode,FALSE,NULL);
    if(c->epoch!=epoch || c->state==E_OWNER_FAILED || c->power!=PowerDeviceD0)s=STATUS_INVALID_DEVICE_STATE;
    else{
        /* Restart is admission only. Actual firmware start is deferred until
         * same-MDL target and an immutable job have been prepared. */
        if(c->rundown_closed){ExReInitializeRundownProtection(&c->operations);c->rundown_closed=FALSE;}
        c->state=E_OWNER_ACTIVE;
        if(!e_native_synchronize(c,reopen_admission,c))s=STATUS_IO_DEVICE_ERROR;
    }
    KeReleaseMutex(&c->mutex,FALSE);return s;
}
static NTSTATUS set_power(void *p,uint64_t epoch,DEVICE_POWER_STATE power)
{
    struct e_native_owner *c=p;NTSTATUS s;struct geo_owner_proof ignored;
    if(!c || (power!=PowerDeviceD0 && power!=PowerDeviceD3))return STATUS_NOT_SUPPORTED;
    if(power==PowerDeviceD3){
        s=quiesce(p,epoch,GEO_QUIESCE_POWER,&ignored);if(!NT_SUCCESS(s))return s;
        KeWaitForSingleObject(&c->mutex,Executive,KernelMode,FALSE,NULL);
        /* Even an unstarted cold graph needs an actual platform OFF for D3. */
        if(!c->hardware_off){
            s=c->platform.Off(c->platform.Header.Context,c->platform_token);
            if(NT_SUCCESS(s)){c->hardware_off=TRUE;s=c->platform.VerifyOff(c->platform.Header.Context,c->platform_token);}
        }
        if(NT_SUCCESS(s))c->power=PowerDeviceD3;else c->state=E_OWNER_FAILED;
    }else{
        if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
        KeWaitForSingleObject(&c->mutex,Executive,KernelMode,FALSE,NULL);
        if(!c->leased || c->epoch!=epoch || c->state!=E_OWNER_QUIESCED)s=STATUS_INVALID_DEVICE_STATE;
        else{s=e_native_platform_on(c);if(NT_SUCCESS(s))c->power=PowerDeviceD0;}
    }
    KeReleaseMutex(&c->mutex,FALSE);return s;
}
static int32_t create(void *p,const struct geo_surface_desc *d,uint64_t *id)
{
    struct e_native_owner *c=p;NTSTATUS s;
    if(!id)return STATUS_INVALID_PARAMETER;*id=0;
    BOOLEAN vertex=d && d->format==100u && d->usage==3u;
    if(vertex){
        if(d->size!=sizeof(*d) || d->version!=1 || d->width<4 || d->width>65536 ||
           d->height!=1 || d->pitch!=d->width || d->reserved)return STATUS_INVALID_PARAMETER;
    }else if(!d || d->size!=sizeof(*d) || d->version!=1 || d->width!=800 || d->height!=600 ||
       d->pitch!=3200 || d->format!=GEO_FORMAT_A8R8G8B8 || d->usage!=GEO_USAGE_RENDER_TARGET || d->reserved)return STATUS_NOT_SUPPORTED;
    s=enter(c,FALSE);if(!NT_SUCCESS(s))return s;
    if((vertex?c->vertex:c->surface) || c->next_id==UINT64_MAX)return leave(c,STATUS_DEVICE_BUSY);
    if(vertex){c->vertex_description=*d;c->vertex=++c->next_id;*id=c->vertex;return leave(c,STATUS_SUCCESS);}
    c->description=*d;c->surface=++c->next_id;*id=c->surface;return leave(c,STATUS_SUCCESS);
}
static int32_t bind(void *p,uint64_t surface,void *mdl,uint32_t offset,uint32_t bytes,uint64_t aperture)
{
    struct e_native_owner *c=p;NTSTATUS s=enter(c,FALSE);if(!NT_SUCCESS(s))return s;
    BOOLEAN vertex=surface && surface==c->vertex;
    if(vertex){
        if(offset || bytes!=c->vertex_description.width || c->allocation->vertex || c->job_state)return leave(c,STATUS_INVALID_PARAMETER);
        s=e_native_build_graph_locked(c);
        if(NT_SUCCESS(s))s=a04_native_allocation_bind_frontend_vertex_cold(c->allocation,&c->memory,mdl,bytes,aperture);
        if(NT_SUCCESS(s))c->vertex_aperture_offset=aperture;
        else if(c->allocation->vertex || c->allocation->frontend_orphan)c->state=E_OWNER_FAILED;
        return leave(c,s);
    }
    if(surface!=c->surface || !surface || offset || bytes!=1920000 || c->allocation->binding || c->job_state)return leave(c,STATUS_INVALID_PARAMETER);
    s=e_native_build_graph_locked(c);
    if(NT_SUCCESS(s))s=a04_native_allocation_bind_frontend_cold(c->allocation,&c->memory,mdl,bytes,aperture,c->colour);
    if(NT_SUCCESS(s))c->aperture_offset=aperture;
    else if(c->allocation->binding || c->allocation->frontend_orphan)c->state=E_OWNER_FAILED;
    return leave(c,s);
}
static int32_t unbind(void *p,uint64_t surface)
{
    struct e_native_owner *c=p;NTSTATUS s=enter(c,TRUE);if(!NT_SUCCESS(s))return s;
    BOOLEAN vertex=surface && surface==c->vertex;
    if(!surface || (!vertex && surface!=c->surface))return leave(c,STATUS_INVALID_PARAMETER);
    s=e_native_stop_locked(c);if(NT_SUCCESS(s))s=e_native_retire_graph_locked(c);
    if(NT_SUCCESS(s)){
        struct a04_native_mdl_binding **b=vertex?&c->allocation->vertex:&c->allocation->binding;
        if(*b)s=a04_native_mdl_release_cold(b);
    }
    if(NT_SUCCESS(s)){c->job=0;c->job_state=E_JOB_NONE;if(vertex)c->vertex_aperture_offset=0;else c->aperture_offset=0;}
    else c->state=E_OWNER_FAILED;
    return leave(c,s);
}
static int32_t destroy(void *p,uint64_t surface)
{
    struct e_native_owner *c=p;NTSTATUS s=enter(c,TRUE);if(!NT_SUCCESS(s))return s;
    if(surface && surface==c->vertex){
        if(c->allocation->vertex || c->job_state)return leave(c,STATUS_DEVICE_BUSY);
        c->vertex=0;RtlZeroMemory(&c->vertex_description,sizeof(c->vertex_description));return leave(c,STATUS_SUCCESS);
    }
    if(!surface || surface!=c->surface)return leave(c,STATUS_INVALID_PARAMETER);
    if(c->allocation->binding || c->job_state)return leave(c,STATUS_DEVICE_BUSY);
    c->surface=0;RtlZeroMemory(&c->description,sizeof(c->description));return leave(c,STATUS_SUCCESS);
}
static int32_t prepare(void *p,uint64_t surface,uint32_t colour,uint64_t *job)
{
    struct e_native_owner *c=p;RGXFWIF_CCB_CTL k;NTSTATUS s;
    if(!job)return STATUS_INVALID_PARAMETER;*job=0;s=enter(c,FALSE);if(!NT_SUCCESS(s))return s;
    if(!surface || surface!=c->surface || !c->allocation->binding || c->next_id==UINT64_MAX ||
       (c->job_state!=E_JOB_NONE && c->job_state!=E_JOB_COMPLETE))return leave(c,STATUS_DEVICE_BUSY);
    /* No live rewrite of USC/PDS. Rebuild under the same adapter/retained MDL
     * after real normal retirement; subsequent boot has a new hardware epoch. */
    s=e_native_stop_locked(c);if(NT_SUCCESS(s))s=e_native_retire_graph_locked(c);
    c->colour=colour;
    if(NT_SUCCESS(s))s=e_native_build_graph_locked(c);
    if(NT_SUCCESS(s))s=a04_vendor_native_recolour_bound_cold(c->allocation->native,colour);
    if(NT_SUCCESS(s))s=e_native_boot_locked(c);
    if(NT_SUCCESS(s)){
        if(a04_rgx_device_read_words(&k,c->allocation->firmware.runtime->kctl,sizeof(k)) ||
           k.ui32WrapMask!=127 || k.ui32ReadOffset>127 || k.ui32ReadOffset!=k.ui32WriteOffset)s=STATUS_DEVICE_BUSY;
        else{c->kick_write=k.ui32WriteOffset;c->job=++c->next_id;c->job_state=E_JOB_PREPARED;*job=c->job;}
    }
    if(!NT_SUCCESS(s))c->state=E_OWNER_FAILED;
    return leave(c,s);
}
static int32_t discard(void *p,uint64_t job)
{
    struct e_native_owner *c=p;NTSTATUS s=enter(c,TRUE);if(!NT_SUCCESS(s))return s;
    if(job!=c->job || !job || c->job_state!=E_JOB_PREPARED)return leave(c,STATUS_DEVICE_BUSY);
    s=e_native_stop_locked(c);if(NT_SUCCESS(s))s=e_native_retire_graph_locked(c);
    if(NT_SUCCESS(s)){c->job=0;c->job_state=E_JOB_NONE;}else c->state=E_OWNER_FAILED;
    return leave(c,s);
}
static int32_t submit(void *p,uint64_t job,uint32_t fence,uint32_t generation)
{
    struct e_native_owner *c=p;NTSTATUS s;
    if(!c || KeGetCurrentIrql()!=c->irq_level || c->closing || !c->leased || !c->irq_gate ||
       c->state!=E_OWNER_ACTIVE || c->power!=PowerDeviceD0 || !job || job!=c->job ||
       c->job_state!=E_JOB_PREPARED || !fence || !generation)return STATUS_INVALID_DEVICE_STATE;
    s=a04_native_dispatch_submit(&c->allocation->dispatch,job,generation,c->kick_write);
    if(NT_SUCCESS(s)){c->generation=generation;c->job_state=E_JOB_SUBMITTED;}
    else{c->state=E_OWNER_FAILED;InterlockedExchange(&c->closing,1);}
    return s;
}
static NTSTATUS interrupt_event(void *p,ULONG message,struct geo_owner_irq_event *out)
{
    struct e_native_owner *c=p;struct a04_native_owner_event event;struct a04_native_irq_receipt drain;
    if(!out)return STATUS_INVALID_PARAMETER;RtlZeroMemory(out,sizeof(*out));out->size=sizeof(*out);out->version=2;
    if(!c || message || KeGetCurrentIrql()!=c->irq_level || !c->irq_gate || !c->allocation)return STATUS_NOT_FOUND;
    out->epoch=c->epoch;
    if(c->allocation->irq.draining){
        if(!a04_native_irq_claim(&c->allocation->irq,&drain))return STATUS_NOT_FOUND;
        out->status=drain.fault;return NT_SUCCESS(drain.fault)?STATUS_PENDING:drain.fault;
    }
    if(!a04_native_dispatch_interrupt(&c->allocation->dispatch,&event))return STATUS_NOT_FOUND;
    if(!NT_SUCCESS(event.status)){out->status=event.status;return event.status;}
    if(!event.completed)return STATUS_PENDING;
    if(event.epoch!=c->hardware_epoch || event.job!=c->job || event.generation!=c->generation || c->job_state!=E_JOB_SUBMITTED)
        return STATUS_INVALID_DEVICE_STATE;
    c->job_state=E_JOB_COMPLETE;out->job=c->job;out->generation=c->generation;
    out->visibility=GEO_COMPLETION_GPU_DONE|GEO_COMPLETION_CPU_VISIBLE;return STATUS_SUCCESS;
}
static NTSTATUS service(void *p,uint64_t epoch)
{
    struct e_native_owner *c=p;NTSTATUS s=enter(c,FALSE);if(!NT_SUCCESS(s))return s;
    if(c->epoch!=epoch || !c->irq_gate)return leave(c,STATUS_INVALID_DEVICE_STATE);
    s=a04_native_dispatch_service(&c->allocation->dispatch);return leave(c,s);
}
static NTSTATUS acquire(void *p,struct geo_owner_lease *out)
{
    struct e_native_owner *c=p;
    if(!c || !out || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(out,sizeof(*out));KeWaitForSingleObject(&c->mutex,Executive,KernelMode,FALSE,NULL);
    if(c->leased || c->state==E_OWNER_FAILED){KeReleaseMutex(&c->mutex,FALSE);return STATUS_DEVICE_BUSY;}
    c->leased=TRUE;reference(c);out->size=sizeof(*out);out->version=2;out->cookie=c;out->epoch=c->epoch;
    out->irq_route=GEO_IRQ_DXGK_REGISTERED;out->irq_level=c->irq_level;
    out->ops=(struct geo_owner_ops){sizeof(out->ops),2,GEO_OWNER_REQUIRED,0,c,create,bind,unbind,destroy,prepare,discard,submit};
    KeReleaseMutex(&c->mutex,FALSE);return STATUS_SUCCESS;
}
static NTSTATUS release(void *p,uint64_t epoch)
{
    struct e_native_owner *c=p;NTSTATUS s=transition_begin(c,epoch);if(!NT_SUCCESS(s))return s;
    if(c->surface || c->vertex || c->job_state || c->allocation->vertex || c->allocation->binding || c->state!=E_OWNER_QUIESCED || !c->receipt_valid)s=STATUS_DEVICE_BUSY;
    else{c->leased=FALSE;dereference(c);s=STATUS_SUCCESS;}
    KeReleaseMutex(&c->mutex,FALSE);return s;
}
NTSTATUS GeoNativeOwnerCreateV3(PDEVICE_OBJECT pdo,const DXGKRNL_INTERFACE *dxgk,
 const DXGK_DEVICE_INFO *info,const struct geo_frontend_memory *memory,struct geo_native_binding *out)
{
    struct e_native_owner *c;struct a04_native_target_aperture aperture;NTSTATUS s;
    if(!out)return STATUS_INVALID_PARAMETER;RtlZeroMemory(out,sizeof(*out));
    if(!pdo || !dxgk || !info || !memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->resolve || !memory->context || !dxgk->DxgkCbSynchronizeExecution || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    c=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*c),E_FACTORY_TAG);if(!c)return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(c,sizeof(*c));c->dxgk=*dxgk;c->memory=*memory;c->references=1;c->epoch=1;
    c->closing=1;c->state=E_OWNER_ACTIVE;c->power=PowerDeviceD0;c->colour=0xff000000;
    KeInitializeMutex(&c->mutex,0);ExInitializeRundownProtection(&c->operations);
    /* Publish only retained context on failure. Never expose partial ops/caps. */
    out->native_context=c;
    s=e_native_map_resources(c,pdo,info);if(!NT_SUCCESS(s))goto fail;
    s=e_native_platform_open(c);if(!NT_SUCCESS(s))goto fail;
    s=e_native_load_firmware(c);if(!NT_SUCCESS(s))goto fail;
    s=a04_native_allocation_create(&c->allocation,pdo,c->firmware_image,c->firmware_bytes,NULL);if(!NT_SUCCESS(s))goto fail;
    s=a04_native_allocation_aperture(c->allocation,&aperture);if(!NT_SUCCESS(s))goto fail;
    out->size=sizeof(*out);out->version=2;
    out->aperture=(struct geo_aperture_config){sizeof(out->aperture),1,aperture.gpu_base,aperture.bytes,aperture.commit_limit,aperture.highest_dma_address};
    out->control.Interface.Size=sizeof(out->control);out->control.Interface.Version=2;
    out->control.Interface.Context=c;out->control.Interface.InterfaceReference=reference;out->control.Interface.InterfaceDereference=dereference;
    out->control.acquire=acquire;out->control.quiesce=quiesce;out->control.reset=reset;out->control.restart=restart;
    out->control.set_power=set_power;out->control.release=release;out->control.interrupt_event=interrupt_event;
    out->control.service=service;out->control.synchronize=e_native_synchronize;
    return STATUS_SUCCESS;
fail:
    c->state=E_OWNER_FAILED;(void)GeoNativeOwnerDelete(out);return s;
}
NTSTATUS GeoNativeOwnerDelete(struct geo_native_binding *out)
{
    struct e_native_owner *c;NTSTATUS s;
    if(!out || !(c=out->native_context) || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    if(c->leased || c->references!=1 || c->surface || c->vertex || c->job_state)return STATUS_DEVICE_BUSY;
    if(c->core && !e_native_synchronize(c,close_admission,c))return STATUS_IO_DEVICE_ERROR;
    if(!c->rundown_closed){ExWaitForRundownProtectionRelease(&c->operations);c->rundown_closed=TRUE;}
    if(c->allocation){
        if(c->allocation->firmware.hardware_owned || c->allocation->start.attempted)return STATUS_DEVICE_BUSY;
        s=a04_native_allocation_release_cold(&c->allocation);if(!NT_SUCCESS(s))return s;
    }
    s=e_native_platform_close(c);if(!NT_SUCCESS(s))return s;
    if(c->firmware_image)ExFreePoolWithTag(c->firmware_image,E_FIRMWARE_TAG);
    if(c->mfg)MmUnmapIoSpace((void *)c->mfg,0x1000);if(c->core)MmUnmapIoSpace((void *)c->core,0x80000);
    ExFreePoolWithTag(c,E_FACTORY_TAG);RtlZeroMemory(out,sizeof(*out));return STATUS_SUCCESS;
}
