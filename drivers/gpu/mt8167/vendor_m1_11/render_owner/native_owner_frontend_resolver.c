/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_owner_frontend_resolver.h"
#include "display_geometry356.h"
#include "native_owner_allocation_private.h"
#define TAG 0x724e4741u
static NTSTATUS resolve_bind(struct a04_native_owner_allocation *a,
    const struct geo_frontend_memory *memory,PMDL original,ULONG active,
    uint64_t offset,uint32_t color,BOOLEAN vertex)
{
    struct geo_frontend_mdl_lease *lease;struct a04_native_mdl_lease normal;NTSTATUS status;
    if(!a || !memory || memory->size!=sizeof(*memory) || memory->version!=GEO_FRONTEND_MEMORY_VERSION ||
       !memory->context || !memory->resolve || !original || (vertex?(active<4u || active>65536u):active!=A04_NATIVE_FRAME_BYTES) ||
       !a->native || !a->adapter || (vertex?a->vertex!=NULL:a->binding!=NULL) || a->frontend_orphan ||
       a->firmware.hardware_owned || a->start.attempted || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    /* Allocate the retention slot before resolve. A broken source-linked
     * resolver's successful but unreleaseable reference must not be lost. */
    lease=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*lease),TAG);
    if(!lease)return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(lease,sizeof(*lease));
    status=memory->resolve(memory->context,original,active,lease);
    if(status!=STATUS_SUCCESS){ExFreePoolWithTag(lease,TAG);return status;}
    if(!lease->mdl || !lease->context || !lease->release){
        a->frontend_orphan=lease;return STATUS_INVALID_DEVICE_STATE;
    }
    if(lease->size!=sizeof(*lease) || lease->version!=GEO_FRONTEND_MEMORY_VERSION ||
       lease->origin!=GEO_MDL_VIDMM_CACHED_SHADOW || lease->active_bytes!=active ||
       lease->page_bytes!=((active+4095u)&~4095u) || lease->mdl==original || !lease->acquire){
        status=STATUS_INVALID_PARAMETER;goto drop;
    }
    normal.context=lease->context;normal.acquire=lease->acquire;normal.release=lease->release;
    if(vertex){
        status=a04_native_mdl_bind_input(&a->vertex,a->adapter,a->pdo,lease->mdl,lease->page_bytes,&normal);
        if(NT_SUCCESS(status))status=a04_vendor_native_attach_vertex_cold(a->native,a->vertex,offset);
    }else status=a04_native_allocation_bind_lease_cold(a,lease->mdl,offset,color,&normal);
    /* Resolve owns one reference; native bind owns a distinct acquire.
     * Even attach failure may keep a native cold reference, never drop it
     * here or unlock the original VidMm MDL speculatively. */
drop:
    lease->release(lease->context,lease->mdl);
    ExFreePoolWithTag(lease,TAG);return status;
}

NTSTATUS a04_native_allocation_bind_frontend_cold(struct a04_native_owner_allocation *a,
 const struct geo_frontend_memory *m,PMDL original,ULONG active,uint64_t offset,uint32_t color)
{return resolve_bind(a,m,original,active,offset,color,FALSE);}
NTSTATUS a04_native_allocation_bind_frontend_vertex_cold(struct a04_native_owner_allocation *a,
 const struct geo_frontend_memory *m,PMDL original,ULONG active,uint64_t offset)
{return resolve_bind(a,m,original,active,offset,0,TRUE);}

/* Same independently retained VidMm PFNs as RT/vertex; no private buffer. */
NTSTATUS a04_native_allocation_bind_frontend_linear_cold(struct a04_native_owner_allocation *a,
 const struct geo_frontend_memory *memory,PMDL original,ULONG active,BOOLEAN input,
 struct a04_native_mdl_binding **out)
{
 struct geo_frontend_mdl_lease *lease;struct a04_native_mdl_lease normal;NTSTATUS status;
 if(!a||!a->adapter||!out||*out||!memory||memory->size!=sizeof(*memory)||memory->version!=1||
    !memory->context||!memory->resolve||!original||!active||active>4194304u||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lease=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*lease),TAG);if(!lease)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(lease,sizeof(*lease));status=memory->resolve(memory->context,original,active,lease);
 if(status!=STATUS_SUCCESS){ExFreePoolWithTag(lease,TAG);return status;}
 if(!lease->mdl||!lease->context||!lease->release){a->frontend_orphan=lease;return STATUS_INVALID_DEVICE_STATE;}
 if(lease->size!=sizeof(*lease)||lease->version!=1||lease->origin!=GEO_MDL_VIDMM_CACHED_SHADOW||
    lease->active_bytes!=active||lease->page_bytes!=((active+4095u)&~4095u)||lease->mdl==original||!lease->acquire){status=STATUS_INVALID_PARAMETER;goto drop_linear;}
 normal.context=lease->context;normal.acquire=lease->acquire;normal.release=lease->release;
 status=input?a04_native_mdl_bind_input(out,a->adapter,a->pdo,lease->mdl,lease->page_bytes,&normal):
              a04_native_mdl_bind(out,a->adapter,a->pdo,lease->mdl,lease->page_bytes,&normal);
 drop_linear:lease->release(lease->context,lease->mdl);ExFreePoolWithTag(lease,TAG);return status;
}
