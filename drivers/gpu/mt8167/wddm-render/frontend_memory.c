/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "kmd_clear.h"
#include "frontend_memory.h"
struct geo_mdl_record {
 struct geo_mdl_record *next;struct geo_kmd_adapter *adapter;
 struct geo_allocation *allocation;PMDL source,identity,view,pinned;
 ULONG mdl_offset_pages;
 ULONG active_bytes,page_bytes,native_refs;UINT map_ref,fault;
};
static void lock(struct geo_kmd_adapter *a){KeWaitForSingleObject(&a->memory_lock,Executive,KernelMode,FALSE,NULL);}
static void unlock(struct geo_kmd_adapter *a){KeReleaseMutex(&a->memory_lock,FALSE);}
static struct geo_mdl_record *mapped(struct geo_kmd_adapter *a,struct geo_allocation *allocation) {
 struct geo_mdl_record *r;for(r=a->memory_records;r;r=r->next)if(r->allocation==allocation&&r->map_ref)return r;return NULL;
}
/* memory_lock held; no GPU/native/ISR callback under this lock. */
static BOOLEAN unlink_unused(struct geo_mdl_record *r) {
 struct geo_mdl_record **link;struct geo_kmd_adapter *a=r->adapter;
 if(r->map_ref||r->native_refs||r->fault||InterlockedCompareExchange(&a->memory_fault,0,0))return FALSE;
 for(link=&a->memory_records;*link&&*link!=r;link=&(*link)->next){}
 if(!*link){InterlockedExchange(&a->memory_fault,1);return FALSE;}
 *link=r->next;InterlockedDecrement(&a->memory_record_count);return TRUE;
}
static void free_record(struct geo_mdl_record *r){GeoUnpinVidMmSpan(r->view,r->pinned);ExFreePoolWithTag(r,'rMeG');}
static NTSTATUS acquire(void *context,PMDL mdl) {
 struct geo_mdl_record *r=context;struct geo_kmd_adapter *a;NTSTATUS status=STATUS_INVALID_PARAMETER;
 if(!r||!(a=r->adapter)||KeGetCurrentIrql()!=PASSIVE_LEVEL)return status;
 lock(a);
 if(!r->fault&&!InterlockedCompareExchange(&a->memory_fault,0,0)&&r->map_ref&&r->pinned==mdl&&r->native_refs<MAXULONG) {
  r->native_refs++;status=STATUS_SUCCESS;
 }
 unlock(a);return status;
}
static void release(void *context,PMDL mdl) {
 struct geo_mdl_record *r=context;struct geo_kmd_adapter *a;BOOLEAN dispose=FALSE;
 if(!r||!(a=r->adapter))return;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL){InterlockedExchange(&a->memory_fault,1);return;}
 lock(a);
 if(r->pinned!=mdl||!r->native_refs){r->fault=1;InterlockedExchange(&a->memory_fault,1);}
 else {r->native_refs--;dispose=unlink_unused(r);}
 unlock(a);if(dispose)free_record(r);
}
static NTSTATUS resolve(void *context,PMDL source,ULONG active,struct geo_frontend_mdl_lease *out) {
 struct geo_kmd_adapter *a=context;struct geo_mdl_record *r;NTSTATUS status=STATUS_NOT_FOUND;
 if(!a||!source||!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(a);
 for(r=a->memory_records;r;r=r->next)if(r->map_ref&&r->identity==source&&r->active_bytes==active) {
  if(r->fault||InterlockedCompareExchange(&a->memory_fault,0,0)){status=STATUS_DEVICE_NOT_READY;break;}
  RtlZeroMemory(out,sizeof(*out));out->size=sizeof(*out);out->version=1;out->origin=GEO_MDL_VIDMM_CACHED_SHADOW;
  out->active_bytes=active;out->page_bytes=r->page_bytes;out->mdl=r->pinned;out->context=r;
  if(r->native_refs==MAXULONG){status=STATUS_INTEGER_OVERFLOW;break;}
  r->native_refs++; /* Resolve itself returns one owned physical-page lease. */
  out->acquire=acquire;out->release=release;status=STATUS_SUCCESS;break;
 }
 unlock(a);return status;
}
void GeoInitializeFrontendMemory(struct geo_kmd_adapter *a) {
 KeInitializeMutex(&a->memory_lock,0);a->memory_api.size=sizeof(a->memory_api);a->memory_api.version=1;
 a->memory_api.context=a;a->memory_api.resolve=resolve;a->memory_ready=1;
}
static NTSTATUS begin_span(struct geo_kmd_adapter *a,struct geo_allocation *allocation,PMDL source,
 ULONG offset_pages,SIZE_T mapped_pages,ULONG active,PMDL *selected) {
 struct geo_mdl_record *r;struct geo_allocation *scan;NTSTATUS status;
 if(selected)*selected=NULL;
 if(!a||!a->memory_ready||!allocation||!source||KeGetCurrentIrql()!=PASSIVE_LEVEL||
    !active||active>4194304||a->engine.poisoned||allocation->bound||
    mapped_pages!=(SIZE_T)BYTES_TO_PAGES(active))return STATUS_INVALID_PARAMETER;
 /* Caller serializes allocation lifetime with the session PASSIVE lock.
  * Physical source span validation/pin is independent of source MDL size. */
 for(scan=a->engine.allocations;scan&&scan!=allocation;scan=scan->next){}
 if(!scan||(uint64_t)active!=(uint64_t)allocation->desc.pitch*allocation->desc.height)return STATUS_INVALID_PARAMETER;
 lock(a);
 if(mapped(a,allocation)||InterlockedCompareExchange(&a->memory_fault,0,0)){unlock(a);return STATUS_DEVICE_BUSY;}
 unlock(a);
 r=ExAllocatePoolWithTag(NonPagedPool,sizeof(*r),'rMeG');if(!r)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(r,sizeof(*r));r->adapter=a;r->allocation=allocation;r->source=source;
 r->mdl_offset_pages=offset_pages;r->active_bytes=active;r->page_bytes=ROUND_TO_PAGES(active);
 /* Probe can fault/sleep. Do not hold memory_lock across the WDM pin. */
 status=GeoPinVidMmSpan(source,offset_pages,r->page_bytes,&r->view,&r->pinned);
 if(!NT_SUCCESS(status)){ExFreePoolWithTag(r,'rMeG');return status;}
 r->identity=selected?r->view:source; /* Legacy zero-offset ABI remains usable. */
 lock(a);
 if(mapped(a,allocation)||InterlockedCompareExchange(&a->memory_fault,0,0)){
  unlock(a);free_record(r);return STATUS_DEVICE_BUSY;
 }
 r->map_ref=1;r->next=a->memory_records;a->memory_records=r;InterlockedIncrement(&a->memory_record_count);
 if(selected)*selected=r->view;
 unlock(a);return STATUS_SUCCESS;
}
NTSTATUS GeoBeginVidMmMapSpan(struct geo_kmd_adapter *a,struct geo_allocation *allocation,PMDL source,
 ULONG offset_pages,SIZE_T mapped_pages,ULONG active,PMDL *selected) {
 if(!selected)return STATUS_INVALID_PARAMETER;
 return begin_span(a,allocation,source,offset_pages,mapped_pages,active,selected);
}
NTSTATUS GeoBeginVidMmMap(struct geo_kmd_adapter *a,struct geo_allocation *allocation,PMDL source,ULONG active) {
 return begin_span(a,allocation,source,0,(SIZE_T)BYTES_TO_PAGES(active),active,NULL);
}
NTSTATUS GeoConfirmVidMmBind(struct geo_kmd_adapter *a,struct geo_allocation *allocation) {
 struct geo_mdl_record *r;NTSTATUS status=STATUS_INVALID_DEVICE_STATE;
 lock(a);r=mapped(a,allocation);
 if(r&&!r->fault&&r->native_refs&&!InterlockedCompareExchange(&a->memory_fault,0,0))status=STATUS_SUCCESS;
 unlock(a);return status; /* A nominal bind without actual lease acquire fails. */
}
void GeoEndVidMmMap(struct geo_kmd_adapter *a,struct geo_allocation *allocation) {
 struct geo_mdl_record *r;BOOLEAN dispose=FALSE;
 if(!a||!a->memory_ready||KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
 lock(a);r=mapped(a,allocation);
 if(r){r->map_ref=0;dispose=unlink_unused(r);}
 unlock(a);if(dispose)free_record(r);
}
