/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_owner_frontend_resolver.h"
#include "native_owner_allocation_private.h"
#define TAG 0x724e4741u
NTSTATUS a04_native_allocation_bind_frontend_cold(struct a04_native_owner_allocation *a,
    const struct geo_frontend_memory *memory,PMDL original,ULONG active,
    uint64_t offset,uint32_t color)
{
    struct geo_frontend_mdl_lease *lease;struct a04_native_mdl_lease normal;NTSTATUS status;
    if(!a || !memory || memory->size!=sizeof(*memory) || memory->version!=GEO_FRONTEND_MEMORY_VERSION ||
       !memory->context || !memory->resolve || !original || active!=1920000u ||
       !a->native || !a->adapter || a->binding || a->frontend_orphan ||
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
       lease->page_bytes!=1921024u || lease->mdl==original || !lease->acquire){
        status=STATUS_INVALID_PARAMETER;goto drop;
    }
    normal.context=lease->context;normal.acquire=lease->acquire;normal.release=lease->release;
    status=a04_native_allocation_bind_lease_cold(a,lease->mdl,offset,color,&normal);
    /* Resolve owns one reference; native bind owns a distinct acquire.
     * Even attach failure may keep a native cold reference, never drop it
     * here or unlock the original VidMm MDL speculatively. */
drop:
    lease->release(lease->context,lease->mdl);
    ExFreePoolWithTag(lease,TAG);return status;
}
