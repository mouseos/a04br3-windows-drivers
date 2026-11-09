/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Source-linked Dxgk owner. No WDF, second IRQ connection or user payload. */
#include "native_owner_factory_private.h"
#include "native_factory_checkpoint.h"
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
static struct e_native_resource *resource(struct e_native_owner *c,uint64_t id)
{struct e_native_resource *r;for(r=c->resources;r && r->id!=id;r=r->next){}return r;}
static int32_t create(void *p,const struct geo_surface_desc *d,uint64_t *id)
{
    struct e_native_owner *c=p;struct e_native_resource *r;NTSTATUS s;
    if(!id)return STATUS_INVALID_PARAMETER;*id=0;
    BOOLEAN vertex=d && d->format==100u && d->usage==3u;
    BOOLEAN linear=d && (d->usage==GEO_USAGE_STAGING || d->usage==GEO_USAGE_INDEX_BUFFER || d->usage==GEO_USAGE_TEXTURE2D);
    if(linear){if(!geo_desc_valid(d))return STATUS_INVALID_PARAMETER;}
    else
    if(vertex){
        if(d->size!=sizeof(*d) || d->version!=1 || d->width<4 || d->width>65536 ||
           d->height!=1 || d->pitch!=d->width || d->reserved)return STATUS_INVALID_PARAMETER;
    }else if(!d || d->size!=sizeof(*d) || d->version!=1 || d->width!=800 || d->height!=600 ||
       d->pitch!=3200 || d->format!=GEO_FORMAT_A8R8G8B8 || d->usage!=GEO_USAGE_RENDER_TARGET || d->reserved)return STATUS_NOT_SUPPORTED;
    s=enter(c,FALSE);if(!NT_SUCCESS(s))return s;
    if(c->next_id==UINT64_MAX)return leave(c,STATUS_INTEGER_OVERFLOW);
    /* Logical allocations are not current hardware bindings. In particular,
     * the runtime's implicit backbuffer must not prevent CreateRenderTarget.
     * Residency is bounded separately by the actual one-target aperture. */
    r=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*r),E_FACTORY_TAG);
    if(!r)return leave(c,STATUS_INSUFFICIENT_RESOURCES);
    RtlZeroMemory(r,sizeof(*r));r->desc=*d;r->id=++c->next_id;
    r->next=c->resources;c->resources=r;*id=r->id;return leave(c,STATUS_SUCCESS);
}
/* Capture scheduler/native progress under the real Dxgk IRQ lock.  Persist
 * only at PASSIVE, never turn this observation into a completion. */
struct native_progress {struct e_native_owner *c;ULONG job,dispatch,serial,generation;};
static BOOLEAN read_native_progress(void *p) {
 struct native_progress *x=p;x->job=x->c->job_state;
 x->dispatch=x->c->allocation->dispatch.state;x->serial=(ULONG)x->c->allocation->irq.serial;
 x->generation=x->c->generation;return TRUE;
}
static void native_progress(struct e_native_owner *c,ULONG stage,NTSTATUS status) {
 struct native_progress x={c,0,0,0,0};
 if(e_native_synchronize(c,read_native_progress,&x))
  GeoFactoryCheckpoint(stage,status,x.job,x.dispatch,x.serial,x.generation);
}
static NTSTATUS wait_boundary_locked(struct e_native_owner *c)
{
    ULONGLONG began=KeQueryInterruptTime();LARGE_INTEGER delay;
    delay.QuadPart=-10000; /* PASSIVE 1ms wait, not a stall or fake ISR. */
    for(;;){
        if(c->closing || c->state!=E_OWNER_ACTIVE)return STATUS_DEVICE_BUSY;
        if(c->allocation->irq.faulted || c->allocation->dispatch.state==A04_NATIVE_DISPATCH_FAULT)return STATUS_IO_DEVICE_ERROR;
        if(c->job_state==E_JOB_NONE || c->job_state==E_JOB_COMPLETE)return STATUS_SUCCESS;
        if(c->job_state!=E_JOB_PREPARED && c->job_state!=E_JOB_SUBMITTED)return STATUS_INVALID_DEVICE_STATE;
        if(KeQueryInterruptTime()-began>=20000000u)return STATUS_IO_TIMEOUT;
        /* The actual worker needs this mutex for SLC/GP/cache + fresh IRQ
         * arming. Keep our rundown ref, but release mutex while it progresses.
         * Submit/ISR use their real Dxgk lock and do not wait for this mutex. */
        KeReleaseMutex(&c->mutex,FALSE);
        (void)KeDelayExecutionThread(KernelMode,FALSE,&delay);
        KeWaitForSingleObject(&c->mutex,Executive,KernelMode,FALSE,NULL);
    }
}
static NTSTATUS cold_for_bind(struct e_native_owner *c)
{
    NTSTATUS s=wait_boundary_locked(c);if(!NT_SUCCESS(s))return s;
    if(c->job_state==E_JOB_NONE)return STATUS_SUCCESS;
    if(c->job_state!=E_JOB_COMPLETE)return STATUS_DEVICE_BUSY;
    /* Clear's retained target remains bound while the first real converted
     * vertex allocation arrives. Completion is not NoDMA: perform the same
     * normal stop/platform-off/IRQ close before editing any cold mapping. */
    s=e_native_stop_locked(c);if(NT_SUCCESS(s))s=e_native_retire_graph_locked(c);
    if(NT_SUCCESS(s))c->job=0;else c->state=E_OWNER_FAILED;
    return s;
}
static int32_t bind(void *p,uint64_t surface,void *mdl,uint32_t offset,uint32_t bytes,uint64_t aperture)
{
    struct e_native_owner *c=p;struct e_native_resource *r;NTSTATUS s;
    GeoFactoryCheckpoint(15,STATUS_SUCCESS,3,(ULONG)surface,bytes,0);
    s=enter(c,FALSE);if(!NT_SUCCESS(s))return s;
    r=resource(c,surface);if(!r || offset)return leave(c,STATUS_INVALID_PARAMETER);
    if(r->desc.usage==GEO_USAGE_STAGING || r->desc.usage==GEO_USAGE_INDEX_BUFFER || r->desc.usage==GEO_USAGE_TEXTURE2D){
        if(r->linear || bytes!=(uint64_t)r->desc.pitch*r->desc.height)return leave(c,STATUS_INVALID_PARAMETER);
        s=cold_for_bind(c);if(!NT_SUCCESS(s))return leave(c,s);
        s=e_native_build_graph_locked(c);if(!NT_SUCCESS(s))return leave(c,s);
        s=a04_native_allocation_bind_frontend_linear_cold(c->allocation,&c->memory,mdl,bytes,r->desc.usage!=GEO_USAGE_STAGING,&r->linear);
        if(NT_SUCCESS(s)){r->linear_offset=aperture;s=a04_vendor_native_attach_linear_cold(c->allocation->native,r->linear,aperture);}
        if(NT_SUCCESS(s))s=e_native_prepare_copy_storage_locked(c);
        if(!NT_SUCCESS(s) && (r->linear || c->allocation->frontend_orphan))c->state=E_OWNER_FAILED;
        return leave(c,s);
    }
    BOOLEAN vertex=r->desc.usage==3u;
    if(vertex){
        if(bytes!=r->desc.width || c->allocation->vertex)return leave(c,STATUS_DEVICE_BUSY);
        s=cold_for_bind(c);if(!NT_SUCCESS(s))return leave(c,s);
        c->vertex=surface;c->vertex_description=r->desc;c->vertex_aperture_offset=aperture;
        s=e_native_build_graph_locked(c);
        if(NT_SUCCESS(s))s=a04_native_allocation_bind_frontend_vertex_cold(c->allocation,&c->memory,mdl,bytes,aperture);
        if(NT_SUCCESS(s))s=e_native_prepare_copy_storage_locked(c);
        if(!NT_SUCCESS(s)){
            if(c->allocation->vertex || c->allocation->frontend_orphan)c->state=E_OWNER_FAILED;
            else c->vertex=0;
        }
        return leave(c,s);
    }
    if(bytes!=1920000 || c->allocation->binding)return leave(c,STATUS_DEVICE_BUSY);
    s=cold_for_bind(c);if(!NT_SUCCESS(s))return leave(c,s);
    c->surface=surface;c->description=r->desc;c->aperture_offset=aperture;
    s=e_native_build_graph_locked(c);
    if(NT_SUCCESS(s))s=a04_native_allocation_bind_frontend_cold(c->allocation,&c->memory,mdl,bytes,aperture,c->colour);
    if(NT_SUCCESS(s))s=e_native_prepare_copy_storage_locked(c);
    if(!NT_SUCCESS(s)){
        if(c->allocation->binding || c->allocation->frontend_orphan)c->state=E_OWNER_FAILED;
        else c->surface=0;
    }
    return leave(c,s);
}
static int32_t unbind(void *p,uint64_t surface)
{
    struct e_native_owner *c=p;NTSTATUS s=enter(c,TRUE);if(!NT_SUCCESS(s))return s;
    struct e_native_resource *linear=resource(c,surface);
    if(linear && linear->linear){
        if(c->job_state==E_JOB_PREPARED || c->job_state==E_JOB_SUBMITTED)return leave(c,STATUS_DEVICE_BUSY);
        s=e_native_stop_locked(c);if(NT_SUCCESS(s))s=e_native_retire_graph_locked(c);
        if(NT_SUCCESS(s))s=a04_native_mdl_release_cold(&linear->linear);
        if(NT_SUCCESS(s)){linear->linear_offset=0;c->job=0;c->job_state=E_JOB_NONE;}else c->state=E_OWNER_FAILED;
        return leave(c,s);
    }
    BOOLEAN vertex=surface && surface==c->vertex;
    if(!surface || (!vertex && surface!=c->surface))return leave(c,STATUS_INVALID_PARAMETER);
    if(c->job_state==E_JOB_PREPARED || c->job_state==E_JOB_SUBMITTED)return leave(c,STATUS_DEVICE_BUSY);
    s=e_native_stop_locked(c);if(NT_SUCCESS(s))s=e_native_retire_graph_locked(c);
    if(NT_SUCCESS(s)){
        struct a04_native_mdl_binding **b=vertex?&c->allocation->vertex:&c->allocation->binding;
        if(*b)s=a04_native_mdl_release_cold(b);
    }
    if(NT_SUCCESS(s)){c->job=0;c->job_state=E_JOB_NONE;if(vertex){c->vertex_aperture_offset=0;c->vertex=0;}else{c->aperture_offset=0;c->surface=0;}}
    else c->state=E_OWNER_FAILED;
    return leave(c,s);
}
static int32_t destroy(void *p,uint64_t surface)
{
    struct e_native_owner *c=p;struct e_native_resource **link,*r;
    NTSTATUS s=enter(c,TRUE);if(!NT_SUCCESS(s))return s;
    for(link=&c->resources;*link && (*link)->id!=surface;link=&(*link)->next){}
    if(!*link)return leave(c,STATUS_INVALID_PARAMETER);r=*link;
    if(r->linear)return leave(c,STATUS_DEVICE_BUSY);
    if((surface==c->vertex && c->allocation->vertex) || (surface==c->surface && c->allocation->binding))return leave(c,STATUS_DEVICE_BUSY);
    *link=r->next;ExFreePoolWithTag(r,E_FACTORY_TAG);return leave(c,STATUS_SUCCESS);
}
static int32_t prepare(void *p,uint64_t surface,uint32_t colour,uint64_t *job)
{
    struct e_native_owner *c=p;RGXFWIF_CCB_CTL k;NTSTATUS s;
    if(!job)return STATUS_INVALID_PARAMETER;*job=0;
    GeoFactoryCheckpoint(15,STATUS_SUCCESS,1,(ULONG)surface,colour,0);
    s=enter(c,FALSE);if(!NT_SUCCESS(s))return s;
    if(!surface || surface!=c->surface || !c->allocation->binding || c->next_id==UINT64_MAX)return leave(c,STATUS_DEVICE_BUSY);
    s=wait_boundary_locked(c);native_progress(c,34,s);if(!NT_SUCCESS(s))return leave(c,s);
    /* A completed Copy leaves a live TQ context and both resident MDLs.
     * Fast clear uses its cold-installed RT EOT, with colour in CCB data.
     * Do not stop/rebuild the FW root just to change that colour. The real
     * transfer completion path still returns cache ownership and a fresh IRQ. */
    if(c->allocation->start.online && c->copy_storage && c->copy_transfer){
        s=a04_native_tq_transfer_rearm_clear(c->copy_transfer,&c->allocation->start,
            c->hardware_epoch,c->next_id+1u,colour);
        if(NT_SUCCESS(s)){
            if(a04_rgx_device_read_words(&k,c->allocation->firmware.runtime->kctl,sizeof(k)) ||
               k.ui32WrapMask!=127 || k.ui32ReadOffset>127 || k.ui32ReadOffset!=k.ui32WriteOffset)s=STATUS_DEVICE_BUSY;
            else{c->kick_write=k.ui32WriteOffset;c->colour=colour;c->job=++c->next_id;
                c->job_is_copy=TRUE; /* existing common TQ submit/finish route */
                c->job_state=E_JOB_PREPARED;*job=c->job;}
        }
        if(!NT_SUCCESS(s)){GeoFactoryCheckpoint(47,s,1,0,0,0);c->state=E_OWNER_FAILED;}
        return leave(c,s);
    }
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
        else{c->kick_write=k.ui32WriteOffset;c->job=++c->next_id;c->job_is_copy=FALSE;c->job_state=E_JOB_PREPARED;*job=c->job;}
    }
    native_progress(c,38,s);
    if(!NT_SUCCESS(s))c->state=E_OWNER_FAILED;
    return leave(c,s);
}
static int32_t prepare_copy(void *p,uint64_t source,uint64_t destination,uint64_t *job) {
 struct e_native_owner *c=p;struct e_native_resource *src,*dst;NTSTATUS s;
 if(!job)return STATUS_INVALID_PARAMETER;*job=0;
 GeoFactoryCheckpoint(15,STATUS_SUCCESS,2,(ULONG)source,(ULONG)destination,0);
 s=enter(c,FALSE);if(!NT_SUCCESS(s))return s;
 src=resource(c,source);dst=resource(c,destination);
 if(!src||!dst||src==dst||source!=c->surface||src->desc.usage!=GEO_USAGE_RENDER_TARGET||
    dst->desc.usage!=GEO_USAGE_STAGING||!dst->linear||!c->allocation->binding||c->next_id==UINT64_MAX)return leave(c,STATUS_INVALID_PARAMETER);
 s=wait_boundary_locked(c);if(!NT_SUCCESS(s))return leave(c,s);
 /* Copy is an idle-to-submit transition on the current FW root. No stop,
  * power cycle, PTE edit, resource release or bootstrap occurs here. */
 /* First use after a real staging MAP may be a genuinely cold graph.
  * Complete its typed assets before evaluating the live-path prerequisites.
  * Never call a cold producer over a hardware-owned/attempted FW root. */
 if(!c->allocation->start.online){
  if(c->allocation->firmware.hardware_owned || c->allocation->start.attempted)return leave(c,STATUS_INVALID_DEVICE_STATE);
  s=e_native_prepare_copy_storage_locked(c);
  if(!NT_SUCCESS(s)){GeoFactoryCheckpoint(19,s,1,0,0,0);c->state=E_OWNER_FAILED;return leave(c,s);}
 }
 if(!c->copy_transfer || c->copy_destination!=destination){GeoFactoryCheckpoint(19,STATUS_NOT_SUPPORTED,2,c->copy_transfer!=NULL,c->copy_destination==destination,0);return leave(c,STATUS_NOT_SUPPORTED);}
 if(!c->allocation->start.online){
  s=e_native_boot_locked(c);if(!NT_SUCCESS(s)){GeoFactoryCheckpoint(19,s,3,0,0,0);c->state=E_OWNER_FAILED;return leave(c,s);}
 }
 s=a04_native_tq_transfer_rearm(c->copy_transfer,&c->allocation->start,c->hardware_epoch,c->next_id+1u);
 if(NT_SUCCESS(s)){RGXFWIF_CCB_CTL k;
  if(a04_rgx_device_read_words(&k,c->allocation->firmware.runtime->kctl,sizeof(k))||k.ui32WrapMask!=127||k.ui32ReadOffset!=k.ui32WriteOffset)s=STATUS_DEVICE_BUSY;
  else{c->kick_write=k.ui32WriteOffset;c->job=++c->next_id;c->job_is_copy=TRUE;c->job_state=E_JOB_PREPARED;*job=c->job;}}
 if(!NT_SUCCESS(s)){GeoFactoryCheckpoint(19,s,4,0,0,0);c->state=E_OWNER_FAILED;}return leave(c,s);
}
struct discard_call {struct e_native_owner *c;uint64_t job;NTSTATUS status;};
static BOOLEAN discard_close(void *p)
{
    struct discard_call *d=p;
    if(d->c->job!=d->job || d->c->job_state!=E_JOB_PREPARED)d->status=STATUS_DEVICE_BUSY;
    else{d->c->job_state=E_JOB_NONE;d->status=STATUS_SUCCESS;}
    return TRUE;
}
static int32_t discard(void *p,uint64_t job)
{
    struct e_native_owner *c=p;NTSTATUS s=enter(c,TRUE);if(!NT_SUCCESS(s))return s;
    struct discard_call call={c,job,STATUS_DEVICE_BUSY};
    if(!job || !e_native_synchronize(c,discard_close,&call))return leave(c,STATUS_IO_DEVICE_ERROR);
    if(!NT_SUCCESS(call.status))return leave(c,call.status);
    s=e_native_stop_locked(c);if(NT_SUCCESS(s))s=e_native_retire_graph_locked(c);
    if(NT_SUCCESS(s)){c->job=0;c->job_state=E_JOB_NONE;}else c->state=E_OWNER_FAILED;
    return leave(c,s);
}
static int32_t submit(void *p,uint64_t job,uint32_t fence,uint32_t generation)
{
    struct e_native_owner *c=p;NTSTATUS s;
    UNREFERENCED_PARAMETER(fence); /* H owns scheduler-fence translation. */
    if(!c || KeGetCurrentIrql()!=c->irq_level || c->closing || !c->leased || !c->irq_gate ||
       c->state!=E_OWNER_ACTIVE || c->power!=PowerDeviceD0 || !job || job!=c->job ||
       c->job_state!=E_JOB_PREPARED || !generation)return STATUS_INVALID_DEVICE_STATE;
    s=c->job_is_copy?a04_native_tq_transfer_submit(c->copy_transfer,&c->allocation->start,&c->allocation->irq,c->kick_write,job,generation):a04_native_dispatch_submit(&c->allocation->dispatch,job,generation,c->kick_write);
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
    if(c->job_is_copy && c->job_state==E_JOB_SUBMITTED){
        NTSTATUS status;LONG phase=a04_native_tq_transfer_state(c->copy_transfer);
        if(!a04_native_irq_claim(&c->allocation->irq,&drain))return STATUS_NOT_FOUND;
        if(!NT_SUCCESS(drain.fault))return drain.fault;
        if(phase==2){status=a04_native_tq_transfer_begin_finish(c->copy_transfer,&c->allocation->irq,&drain);return NT_SUCCESS(status)?STATUS_PENDING:status;}
        if(phase!=6 || !a04_native_tq_transfer_identity(c->copy_transfer,c->hardware_epoch,c->job,c->generation))return STATUS_INVALID_DEVICE_STATE;
        status=a04_native_tq_transfer_visibility_receipt(c->copy_transfer,&c->allocation->start,&c->allocation->irq,&drain);
        if(status!=STATUS_SUCCESS)return status;
        c->job_state=E_JOB_COMPLETE;out->job=c->job;out->generation=c->generation;
        out->visibility=GEO_COMPLETION_GPU_DONE|GEO_COMPLETION_CPU_VISIBLE;return STATUS_SUCCESS;
    }
    if(!a04_native_dispatch_interrupt(&c->allocation->dispatch,&event))return STATUS_NOT_FOUND;
    if(!NT_SUCCESS(event.status)){out->status=event.status;return event.status;}
    if(!event.completed)return STATUS_PENDING;
    if(event.epoch!=c->hardware_epoch || event.job!=c->job || event.generation!=c->generation || c->job_state!=E_JOB_SUBMITTED)
        return STATUS_INVALID_DEVICE_STATE;
    c->job_state=E_JOB_COMPLETE;out->job=c->job;out->generation=c->generation;
    out->visibility=GEO_COMPLETION_GPU_DONE|GEO_COMPLETION_CPU_VISIBLE;return STATUS_SUCCESS;
}
struct copy_arm {struct e_native_owner *c;NTSTATUS status;};
static BOOLEAN copy_arm_visibility(void *p){struct copy_arm *x=p;x->status=a04_native_tq_transfer_arm_visibility(x->c->copy_transfer,&x->c->allocation->start,&x->c->allocation->irq);return TRUE;}
static NTSTATUS service(void *p,uint64_t epoch)
{
    struct e_native_owner *c=p;NTSTATUS s=enter(c,FALSE);if(!NT_SUCCESS(s))return s;
    if(c->epoch!=epoch)return leave(c,STATUS_INVALID_DEVICE_STATE);
    /* A deferred request from the previous hardware graph may have waited
     * on this mutex during rebootstrap. No graph/source remains to service;
     * this retires only the work request, never a GPU completion. */
    if(!c->irq_gate && c->job_state==E_JOB_NONE)return leave(c,STATUS_SUCCESS);
    if(!c->irq_gate)return leave(c,STATUS_INVALID_DEVICE_STATE);
    if(c->job_is_copy&&c->job_state==E_JOB_SUBMITTED){
        struct a04_native_copy_gpu_result result;struct copy_arm arm={c,STATUS_SUCCESS};LONG phase=a04_native_tq_transfer_state(c->copy_transfer);
        if(phase==2||phase==6)return leave(c,STATUS_SUCCESS); /* wait for actual registered IRQ */
        if(phase==8){s=a04_native_tq_transfer_wait_visibility_retry(c->copy_transfer,&c->allocation->start);if(s!=STATUS_SUCCESS)return leave(c,s);}
        if(phase==3){s=a04_native_tq_transfer_finish_gpu(c->copy_transfer,&c->allocation->start,&result);if(s!=STATUS_SUCCESS)return leave(c,s);}
        if(a04_native_tq_transfer_state(c->copy_transfer)!=4)return leave(c,STATUS_INVALID_DEVICE_STATE);
        if(!e_native_synchronize(c,copy_arm_visibility,&arm))return leave(c,STATUS_IO_DEVICE_ERROR);
        return leave(c,arm.status);
    }
    s=a04_native_dispatch_service(&c->allocation->dispatch);native_progress(c,39,s);return leave(c,s);
}
static NTSTATUS acquire(void *p,struct geo_owner_lease *out)
{
    struct e_native_owner *c=p;
    if(!c || !out || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(out,sizeof(*out));KeWaitForSingleObject(&c->mutex,Executive,KernelMode,FALSE,NULL);
    if(c->leased || c->state==E_OWNER_FAILED){KeReleaseMutex(&c->mutex,FALSE);return STATUS_DEVICE_BUSY;}
    c->leased=TRUE;reference(c);out->size=sizeof(*out);out->version=2;out->cookie=c;out->epoch=c->epoch;
    out->irq_route=GEO_IRQ_DXGK_REGISTERED;out->irq_level=c->irq_level;
    out->ops=(struct geo_owner_ops){sizeof(out->ops),GEO_OWNER_OPS_VERSION,GEO_OWNER_REQUIRED,0,c,create,bind,unbind,destroy,prepare,discard,submit,prepare_copy};
    KeReleaseMutex(&c->mutex,FALSE);return STATUS_SUCCESS;
}
static NTSTATUS release(void *p,uint64_t epoch)
{
    struct e_native_owner *c=p;NTSTATUS s=transition_begin(c,epoch);if(!NT_SUCCESS(s))return s;
    if(c->resources || c->job_state || c->allocation->vertex || c->allocation->binding || c->state!=E_OWNER_QUIESCED || !c->receipt_valid)s=STATUS_DEVICE_BUSY;
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
    out->native_context=c;GeoFactoryCheckpoint(0,STATUS_SUCCESS,0,0,0,0);
    s=e_native_map_resources(c,pdo,info);GeoFactoryCheckpoint(1,s,0,0,0,0);if(!NT_SUCCESS(s))goto fail;
    s=e_native_platform_open(c);GeoFactoryCheckpoint(2,s,0,0,0,0);if(!NT_SUCCESS(s))goto fail;
    s=e_native_load_firmware(c);GeoFactoryCheckpoint(3,s,0,0,0,0);if(!NT_SUCCESS(s))goto fail;
    s=a04_native_allocation_create(&c->allocation,pdo,c->firmware_image,c->firmware_bytes,NULL);GeoFactoryCheckpoint(4,s,0,0,0,0);if(!NT_SUCCESS(s))goto fail;
    s=a04_native_allocation_aperture(c->allocation,&aperture);GeoFactoryCheckpoint(5,s,0,0,0,0);if(!NT_SUCCESS(s))goto fail;
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
    if(c->leased || c->references!=1 || c->resources || c->job_state)return STATUS_DEVICE_BUSY;
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

#if A04_GE8300_NATIVE_OWNER_DRAW
#include "windows_native_cold_private.h"
#include "../../wddm-render/frontend/draw_link_abi.h"
#include "../../wddm-render/frontend/texture_draw_abi.h"
#include "../../wddm-render/frontend/shader_draw_abi.h"
static int32_t prepare_draw_impl(void *p,const struct geo_draw_request *request,
 const void *blob,uint32_t bytes,uint64_t *job,const struct geo_draw_request_v2 *linked,const struct geo_draw_request_v3 *sampled,const struct geo_draw_request_v4 *general,const struct geo_draw_request_v5 *combined)
{
    struct e_native_owner *c=p;RGXFWIF_CCB_CTL k;NTSTATUS s;uint64_t end,offset;struct e_native_resource *texture=NULL;
    if(!job)return STATUS_INVALID_PARAMETER;*job=0;
    s=combined?geo_draw_validate_capture_v5(combined,blob,bytes):general?geo_draw_validate_capture_v4(general,blob,bytes):sampled?geo_draw_validate_capture_v3(sampled,blob,bytes):linked?geo_draw_validate_capture_v2(linked,blob,bytes):geo_draw_validate_capture(request,blob,bytes);if(!NT_SUCCESS(s))return s;
    s=enter(c,FALSE);if(!NT_SUCCESS(s))return s;
    if(request->target!=c->surface || request->vertex!=c->vertex || !c->vertex ||
       !c->allocation->binding || !c->allocation->vertex || c->next_id==UINT64_MAX ||
       RtlCompareMemory(&c->description,&request->target_desc,sizeof(c->description))!=sizeof(c->description))return leave(c,STATUS_INVALID_DEVICE_STATE);
    if(sampled){texture=resource(c,sampled->texture);if(!texture||!texture->linear||texture->desc.usage!=GEO_USAGE_TEXTURE2D||RtlCompareMemory(&texture->desc,&sampled->texture_desc,sizeof(texture->desc))!=sizeof(texture->desc))return leave(c,STATUS_INVALID_PARAMETER);}
    s=wait_boundary_locked(c);native_progress(c,34,s);if(!NT_SUCCESS(s))return leave(c,s);
    offset=(uint64_t)request->state.first_vertex*request->state.vertex_stride+request->state.vertex_offset;
    end=offset+(uint64_t)request->state.vertex_count*request->state.vertex_stride;
    if(request->state.vertex_count!=3 || end>c->vertex_description.width)return leave(c,STATUS_INVALID_PARAMETER);
    /* Immutable transformed input is a distinct H-owned VidMm allocation;
     * its independent MDL/SG is pinned through actual fence/retirement. */
    const void *vertex_cpu=a04_native_mdl_cpu(c->allocation->vertex);
    if(!vertex_cpu)return leave(c,STATUS_INVALID_DEVICE_STATE);
    const uint32_t *vertices=(const uint32_t *)((const UCHAR *)vertex_cpu+(size_t)offset);
    for(unsigned i=0;i<(end-offset)/4;i++)if((vertices[i]&0x7f800000u)==0x7f800000u)return leave(c,STATUS_INVALID_PARAMETER);
    /* Preserve the live context only for an exact existing immutable image.
     * A changed image takes the established cold path; no shader/data/PTE
     * is edited live and no previous frame's sentinel proof is reused. */
    if(c->job_state==E_JOB_COMPLETE && c->allocation->start.online){
        s=a04_vendor_native_match_draw_live(c->allocation->native,request,blob,bytes,
            A04_NATIVE_OUTPUT_VA+c->vertex_aperture_offset+offset,linked,sampled,
            texture?A04_NATIVE_OUTPUT_VA+texture->linear_offset:0,general,combined);
        if(s==STATUS_SUCCESS){
            s=a04_vendor_native_owner_rearm_draw(c->allocation->native,&c->allocation->start);
            if(NT_SUCCESS(s))s=a04_native_dispatch_rearm_completed(&c->allocation->dispatch);
            if(NT_SUCCESS(s)){
                if(a04_rgx_device_read_words(&k,c->allocation->firmware.runtime->kctl,sizeof(k))||
                   k.ui32WrapMask!=127||k.ui32ReadOffset>127||k.ui32ReadOffset!=k.ui32WriteOffset)s=STATUS_DEVICE_BUSY;
                else{c->kick_write=k.ui32WriteOffset;c->job=++c->next_id;c->job_is_copy=FALSE;c->job_state=E_JOB_PREPARED;*job=c->job;}
            }
            if(!NT_SUCCESS(s))c->state=E_OWNER_FAILED;
            return leave(c,s);
        }
        if(s!=STATUS_NOT_SUPPORTED){c->state=E_OWNER_FAILED;return leave(c,s);}
    }
    s=e_native_stop_locked(c);if(NT_SUCCESS(s))s=e_native_retire_graph_locked(c);
    if(NT_SUCCESS(s))s=e_native_build_graph_locked(c);
    if(NT_SUCCESS(s))s=combined?a04_vendor_native_prepare_draw_v5_cold(c->allocation->native,combined,blob,bytes,A04_NATIVE_OUTPUT_VA+c->vertex_aperture_offset+offset,A04_NATIVE_OUTPUT_VA+texture->linear_offset):general?a04_vendor_native_prepare_draw_v4_cold(c->allocation->native,general,blob,bytes,A04_NATIVE_OUTPUT_VA+c->vertex_aperture_offset+offset):sampled?a04_vendor_native_prepare_draw_v3_cold(c->allocation->native,sampled,blob,bytes,A04_NATIVE_OUTPUT_VA+c->vertex_aperture_offset+offset,A04_NATIVE_OUTPUT_VA+texture->linear_offset):linked?a04_vendor_native_prepare_draw_v2_cold(c->allocation->native,linked,blob,bytes,
        A04_NATIVE_OUTPUT_VA+c->vertex_aperture_offset+offset):
        a04_vendor_native_prepare_draw_cold(c->allocation->native,request,blob,bytes,
        A04_NATIVE_OUTPUT_VA+c->vertex_aperture_offset+offset);
    if(!NT_SUCCESS(s)){
        /* Unsupported shader/state never publishes a job. A real resource
         * write/retirement failure keeps all backing and closes admission. */
        if(s!=STATUS_NOT_SUPPORTED && s!=STATUS_INVALID_PARAMETER)c->state=E_OWNER_FAILED;
        return leave(c,s);
    }
    s=e_native_boot_locked(c);
    if(NT_SUCCESS(s)){
        if(a04_rgx_device_read_words(&k,c->allocation->firmware.runtime->kctl,sizeof(k)) ||
           k.ui32WrapMask!=127 || k.ui32ReadOffset>127 || k.ui32ReadOffset!=k.ui32WriteOffset)s=STATUS_DEVICE_BUSY;
        else{c->kick_write=k.ui32WriteOffset;c->job=++c->next_id;c->job_is_copy=FALSE;c->job_state=E_JOB_PREPARED;*job=c->job;}
    }
    if(!NT_SUCCESS(s))c->state=E_OWNER_FAILED;
    return leave(c,s);
}
static int32_t prepare_draw(void *p,const struct geo_draw_request *r,const void *blob,uint32_t bytes,uint64_t *job)
{ return prepare_draw_impl(p,r,blob,bytes,job,NULL,NULL,NULL,NULL); }
static int32_t prepare_draw_v2(void *p,const struct geo_draw_request_v2 *r,const void *blob,uint32_t bytes,uint64_t *job)
{
 if(!r){if(job)*job=0;return GEO_INVALID;}
 return prepare_draw_impl(p,&r->base,blob,bytes,job,r,NULL,NULL,NULL);
}
int32_t GeoNativeOwnerGetDrawOpsV1(struct geo_native_binding *binding,struct geo_native_draw_ops_v1 *out)
{
    struct e_native_owner *c;
    if(!out)return GEO_INVALID;RtlZeroMemory(out,sizeof(*out));
    if(!binding || binding->size!=sizeof(*binding) || binding->version!=2 ||
       !(c=binding->native_context) || binding->control.Interface.Context!=c ||
       c->state==E_OWNER_FAILED)return GEO_NOT_READY;
    out->size=sizeof(*out);out->version=1;out->context=c;out->prepare_draw=prepare_draw;
    return GEO_OK;
}
int32_t GeoNativeOwnerGetDrawOpsV2(struct geo_native_binding *binding,struct geo_native_draw_ops_v2 *out)
{
 struct geo_native_draw_ops_v1 v1;int32_t status;
 if(!out)return GEO_INVALID;RtlZeroMemory(out,sizeof(*out));
 status=GeoNativeOwnerGetDrawOpsV1(binding,&v1);if(status)return status;
 out->size=sizeof(*out);out->version=2;out->context=v1.context;out->prepare_draw=prepare_draw_v2;
 return GEO_OK;
}

static int32_t prepare_draw_v3(void *p,const struct geo_draw_request_v3 *r,const void *blob,uint32_t bytes,uint64_t *job) {
 if(!r){if(job)*job=0;return GEO_INVALID;}return prepare_draw_impl(p,&r->base,blob,bytes,job,NULL,r,NULL,NULL);
}
int32_t GeoNativeOwnerGetDrawOpsV3(struct geo_native_binding *b,struct geo_native_draw_ops_v3 *out) {
 struct geo_native_draw_ops_v1 v;int32_t s;if(!out)return GEO_INVALID;RtlZeroMemory(out,sizeof(*out));s=GeoNativeOwnerGetDrawOpsV1(b,&v);if(s)return s;
 out->size=sizeof(*out);out->version=3;out->context=v.context;out->prepare_draw=prepare_draw_v3;return GEO_OK;
}

static int32_t prepare_draw_v4(void *p,const struct geo_draw_request_v4 *r,const void *blob,uint32_t bytes,uint64_t *job)
{if(!r){if(job)*job=0;return GEO_INVALID;}return prepare_draw_impl(p,&r->base,blob,bytes,job,NULL,NULL,r,NULL);}
int32_t GeoNativeOwnerGetDrawOpsV4(struct geo_native_binding *b,struct geo_native_draw_ops_v4 *out) {
 struct geo_native_draw_ops_v1 v;int32_t s;if(!out)return GEO_INVALID;RtlZeroMemory(out,sizeof(*out));
 s=GeoNativeOwnerGetDrawOpsV1(b,&v);if(s)return s;
 out->size=sizeof(*out);out->version=4;out->context=v.context;out->prepare_draw=prepare_draw_v4;return GEO_OK;
}

static int32_t prepare_draw_v5(void *p,const struct geo_draw_request_v5 *r,const void *blob,uint32_t bytes,uint64_t *job)
{if(!r){if(job)*job=0;return GEO_INVALID;}return prepare_draw_impl(p,&r->sampled.base,blob,bytes,job,NULL,&r->sampled,NULL,r);}
int32_t GeoNativeOwnerGetDrawOpsV5(struct geo_native_binding *b,struct geo_native_draw_ops_v5 *out) {
 struct geo_native_draw_ops_v1 v;int32_t s;if(!out)return GEO_INVALID;RtlZeroMemory(out,sizeof(*out));s=GeoNativeOwnerGetDrawOpsV1(b,&v);if(s)return s;
 out->size=sizeof(*out);out->version=5;out->context=v.context;out->prepare_draw=prepare_draw_v5;return GEO_OK;
}
#endif
