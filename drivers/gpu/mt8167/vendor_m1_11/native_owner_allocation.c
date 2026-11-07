/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_owner_allocation_private.h"
#include "native_factory_checkpoint.h"
#define TAG 0x6f4e4741u
NTSTATUS a04_native_allocation_release_cold(struct a04_native_owner_allocation **pointer)
{
    struct a04_native_owner_allocation *a;NTSTATUS status;
    if(!pointer || !(a=*pointer) || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    /* These flags are NOT cleared by power-off ack or IRQ mask. A separately
     * proven future retirement must remove exposure before any DMA release. */
    if(a->adapter && (!a->adapter->DmaOperations || !a->adapter->DmaOperations->PutDmaAdapter))return STATUS_INVALID_DEVICE_STATE;
    if(a->frontend_orphan || a->firmware.hardware_owned || a->start.attempted || a->irq.core ||
       a->dispatch.state)return STATUS_DEVICE_BUSY;
    if(a->native){status=a04_vendor_native_release_cold(&a->native);if(!NT_SUCCESS(status))return status;}
    if(a->vertex){status=a04_native_mdl_release_cold(&a->vertex);if(!NT_SUCCESS(status))return status;}
    if(a->binding){status=a04_native_mdl_release_cold(&a->binding);if(!NT_SUCCESS(status))return status;}
    if(a->render.firmware){status=a04_vendor_render_release_cold(&a->render);if(!NT_SUCCESS(status))return status;}
    status=a04_vendor_m1_11_windows_free(&a->firmware);if(!NT_SUCCESS(status))return status;
    if(a->adapter){a->adapter->DmaOperations->PutDmaAdapter(a->adapter);a->adapter=NULL;}
    if(a->pdo){ObDereferenceObject(a->pdo);a->pdo=NULL;}
    ExFreePoolWithTag(a,TAG);*pointer=NULL;return STATUS_SUCCESS;
}
NTSTATUS a04_native_allocation_create(struct a04_native_owner_allocation **out,
    PDEVICE_OBJECT pdo,const void *image,size_t bytes,const struct a04_native_owner_frontend *frontend)
{
    struct a04_native_owner_allocation *a;DEVICE_DESCRIPTION d;NTSTATUS status;
    if(!out || *out || !pdo || !image || !bytes ||
       (frontend && (frontend->size!=sizeof(*frontend) || frontend->version!=A04_NATIVE_FRONTEND_VERSION ||
       !frontend->normal_mdl.acquire || !frontend->normal_mdl.release)) ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    a=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*a),TAG);
    if(!a)return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(a,sizeof(*a));*out=a;if(frontend)a->frontend=*frontend;
    ObReferenceObject(pdo);a->pdo=pdo;
    RtlZeroMemory(&d,sizeof(d));d.Version=DEVICE_DESCRIPTION_VERSION3;
    d.Master=TRUE;d.ScatterGather=TRUE;d.DmaAddressWidth=32;
    d.InterfaceType=Internal;d.MaximumLength=16u*1024u*1024u;
    a->adapter=IoGetDmaAdapter(pdo,&d,&a->map_registers);
    if(!a->adapter){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
    if(!a->adapter->DmaOperations || !a->adapter->DmaOperations->PutDmaAdapter){
        /* Cannot manufacture a valid release callback. Preserve real adapter
         * and PDO reference for failed-factory retention, do not dereference. */
        return STATUS_INVALID_DEVICE_STATE;
    }
    status=a04_vendor_m1_11_windows_stage_uncached(&a->firmware,a->adapter,image,bytes);
    GeoFactoryCheckpoint(30,status,a->firmware.stage_step,0,0,0);
    if(!NT_SUCCESS(status))goto fail;
    status=a04_vendor_render_prepare_cold(&a->render,&a->firmware,a->adapter,FALSE,0);
    GeoFactoryCheckpoint(31,status,a->firmware.stage_step,0,0,0);
    if(!NT_SUCCESS(status))goto fail;
    status=a04_vendor_native_allocate_cold_ex(&a->native,&a->render,a->adapter,
        a->map_registers,800,600,&a->diagnostic);
    GeoFactoryCheckpoint(32,status,a->diagnostic.stage,a->diagnostic.index,a->diagnostic.request_bytes,a->diagnostic.map_registers);
    GeoFactoryCheckpoint(33,status,a->diagnostic.data_bytes,a->diagnostic.data_blocks,a->diagnostic.pixel_blocks,(ULONG)a->diagnostic.status);
    if(!NT_SUCCESS(status))goto fail;
    if(!a04_vendor_native_job_staged(a->native)){status=STATUS_INVALID_DEVICE_STATE;goto fail;}
    return STATUS_SUCCESS;
fail:
    (void)a04_native_allocation_release_cold(out); /* keep nonnull on release failure */
    return status;
}
NTSTATUS a04_native_allocation_bind_lease_cold(struct a04_native_owner_allocation *a,
    PMDL mdl,uint64_t offset,uint32_t color,const struct a04_native_mdl_lease *lease)
{
    NTSTATUS status;
    if(!a || !mdl || !lease || !lease->acquire || !lease->release || !a->native || !a->adapter || a->binding ||
       a->firmware.hardware_owned || a->start.attempted ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    status=a04_native_mdl_bind(&a->binding,a->adapter,a->pdo,mdl,469u*4096u,
        lease);
    if(!NT_SUCCESS(status))return status;
    status=a04_vendor_native_attach_mdl_target_preserve_cold(a->native,a->binding,offset,color);
    /* On error leave the real binding owned. Native may hold a cold ref after
     * a partial target rebuild; ordered release must drop native first. */
    return status;
}
NTSTATUS a04_native_allocation_bind_cold(struct a04_native_owner_allocation *a,
    PMDL mdl,uint64_t offset,uint32_t color)
{
    if(!a)return STATUS_INVALID_PARAMETER;
    return a04_native_allocation_bind_lease_cold(a,mdl,offset,color,&a->frontend.normal_mdl);
}
NTSTATUS a04_native_allocation_aperture(const struct a04_native_owner_allocation *a,
    struct a04_native_target_aperture *out)
{
    if(!a || !a->native)return STATUS_INVALID_DEVICE_STATE;
    return a04_vendor_native_target_aperture(a->native,out);
}
