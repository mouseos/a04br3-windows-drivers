/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "BDD.hxx"
#include "ge8300_display_resource_check.h"
#include "present_tile.h"
#if A04_GE8300_WDDM_DIRECT_EXPERIMENT
NTSTATUS A04DisplayResources::Start(PDEVICE_OBJECT pdo,const DXGK_DEVICE_INFO *info)
{
    PCM_RESOURCE_LIST list; PCM_PARTIAL_RESOURCE_LIST partial;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR ovl=NULL,irq=NULL;
    ULONG matches=0,interrupts=0,i;DEVICE_DESCRIPTION desc;
    if (!pdo || !info || info->PhysicalDeviceObject!=pdo || Adapter || Ovl ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    list=info->TranslatedResourceList;
    if (!list || list->Count!=1) return STATUS_DEVICE_CONFIGURATION_ERROR;
    partial=&list->List[0].PartialResourceList;
    for (i=0;i<partial->Count;i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR item=&partial->PartialDescriptors[i];
        if (item->Type==CmResourceTypeMemory &&
            (ULONGLONG)item->u.Memory.Start.QuadPart==0x14007000ULL) { ovl=item; matches++; }
        if (item->Type==CmResourceTypeInterrupt) { irq=item; interrupts++; }
    }
    if (!ovl || !a04_display_resources_valid(matches,interrupts,
        ovl->ShareDisposition==CmResourceShareDeviceExclusive,
        irq && irq->ShareDisposition==CmResourceShareDeviceExclusive,
        (uint64_t)ovl->u.Memory.Start.QuadPart,ovl->u.Memory.Length))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    /* A GPIO or unrelated ACPI PDO must never be substituted for the OVL PDO. */
    RtlZeroMemory(&desc,sizeof(desc));desc.Version=DEVICE_DESCRIPTION_VERSION3;
    desc.Master=TRUE;desc.ScatterGather=TRUE;desc.DmaAddressWidth=32;
    desc.InterfaceType=Internal;desc.MaximumLength=4096000;
    Adapter=IoGetDmaAdapter(pdo,&desc,&MapRegisters);
    if (!Adapter || !Adapter->DmaOperations ||
        Adapter->DmaOperations->Size<
            FIELD_OFFSET(DMA_OPERATIONS,PutScatterGatherList)+sizeof(Adapter->DmaOperations->PutScatterGatherList) ||
        !Adapter->DmaOperations->GetScatterGatherList || !Adapter->DmaOperations->PutScatterGatherList ||
        MapRegisters<1000) { Stop(); return STATUS_NOT_SUPPORTED; }
    Ovl=(volatile ULONG *)MmMapIoSpace(ovl->u.Memory.Start,0x1000,MmNonCached);
    if (!Ovl) { Stop(); return STATUS_INSUFFICIENT_RESOURCES; }
    DisplayPdo=pdo;HasInterrupt=(interrupts==1);
    return STATUS_SUCCESS;
}
NTSTATUS A04DisplayResources::PrepareShared(PDMA_ADAPTER gpu,PDEVICE_OBJECT gpuPdo)
{
    if (!Adapter || !DisplayPdo || !Ovl) return STATUS_DEVICE_NOT_READY;
    return a04_shared_scanout_prepare(&Shared,gpu,gpuPdo,Adapter,DisplayPdo);
}
#if A04_GE8300_WDDM_SHARED_LEASE
static NTSTATUS SharedLeaseIoctl(HANDLE handle,ULONG code,struct a04_ge8300_shared_lease *lease)
{
    IO_STATUS_BLOCK io={};NTSTATUS status;PFILE_OBJECT file=NULL;
    status=ObReferenceObjectByHandle(handle,SYNCHRONIZE,*IoFileObjectType,KernelMode,(PVOID *)&file,NULL);
    if(!NT_SUCCESS(status))return status;
    status=ZwDeviceIoControlFile(handle,NULL,NULL,NULL,&io,code,lease,sizeof(*lease),lease,sizeof(*lease));
    if (status==STATUS_PENDING) {
        do {status=KeWaitForSingleObject(file,Executive,KernelMode,FALSE,NULL);} while(status!=STATUS_SUCCESS);
        status=io.Status;
    } else if (NT_SUCCESS(status)) status=io.Status;
    ObDereferenceObject(file);
    if (NT_SUCCESS(status) && io.Information!=sizeof(*lease))return STATUS_IO_DEVICE_ERROR;
    return status;
}
#if A04_GE8300_WDDM_SHARED_PRESENT
NTSTATUS A04DisplayResources::SubmitFullFrame(const void *pixels,uint32_t pitch)
{
    struct a04_present_surface source;struct a04_present_tile position;
    struct a04_ge8300_shared_tile *tile;IO_STATUS_BLOCK io={};PFILE_OBJECT file=NULL;
    NTSTATUS status;uint32_t frame;
    if(!pixels || pitch<3200u || (pitch&3u) || !GpuHandle || !Lease.token ||
       !Shared.mdl || !Shared.exposed || !Shared.gpu_sg || !Shared.display_sg ||
       FrameId==UINT32_MAX || KeGetCurrentIrql()!=PASSIVE_LEVEL || KeAreAllApcsDisabled())
        return STATUS_INVALID_DEVICE_STATE;
    source.base=(unsigned char *)pixels;source.bytes=(size_t)pitch*1280u;
    if(source.bytes/pitch!=1280u)return STATUS_INVALID_PARAMETER;
    source.width=800;source.height=1280;source.pitch=pitch;
    tile=(struct a04_ge8300_shared_tile *)ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*tile),'fG4A');
    if(!tile)return STATUS_INSUFFICIENT_RESOURCES;
    status=ObReferenceObjectByHandle(GpuHandle,SYNCHRONIZE,*IoFileObjectType,KernelMode,(PVOID *)&file,NULL);
    if(!NT_SUCCESS(status)){ExFreePoolWithTag(tile,'fG4A');return status;}
    frame=++FrameId;
    for(uint32_t i=0;i<1000;i++) {
        RtlZeroMemory(tile,sizeof(*tile));tile->token=Lease.token;tile->frame=frame;tile->index=i;
        position.x=(i%25u)*32u;position.y=(i/25u)*32u;
        if(a04_present_tile_pack((unsigned char *)tile->pixels,4096,&source,&position)) {
            status=STATUS_INVALID_PARAMETER;break;
        }
        io.Information=0;io.Status=STATUS_PENDING;
        status=ZwDeviceIoControlFile(GpuHandle,NULL,NULL,NULL,&io,A04_GE8300_IOCTL_SHARED_TILE,
            tile,sizeof(*tile),tile,sizeof(*tile));
        if(status==STATUS_PENDING) {
            do {status=KeWaitForSingleObject(file,Executive,KernelMode,FALSE,NULL);} while(status!=STATUS_SUCCESS);
            status=io.Status;
        } else if(NT_SUCCESS(status))status=io.Status;
        if(!NT_SUCCESS(status) || io.Information!=sizeof(*tile) || !tile->task || !tile->flush) {
            if(NT_SUCCESS(status))status=STATUS_IO_DEVICE_ERROR;break;
        }
    }
    ObDereferenceObject(file);ExFreePoolWithTag(tile,'fG4A');return status;
}
#endif
NTSTATUS A04DisplayResources::SwitchShared(uint32_t expected,a04_shared_ovl_ready ready,
    a04_shared_ovl_boundary boundary,void *context)
{
    return a04_shared_ovl_switch(&SharedOvl,&Shared,&Lease,Ovl,0x1000,expected,ready,boundary,context);
}
NTSTATUS A04DisplayResources::PrepareViaGpu()
{
    UNICODE_STRING name=RTL_CONSTANT_STRING(L"\\Device\\A04GE8300");
    OBJECT_ATTRIBUTES attr;IO_STATUS_BLOCK io={};NTSTATUS status;
    if(!Adapter || !Ovl || !DisplayPdo || GpuHandle || Shared.mdl ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL || KeAreAllApcsDisabled())return STATUS_DEVICE_NOT_READY;
    InitializeObjectAttributes(&attr,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
    status=ZwCreateFile(&GpuHandle,FILE_READ_DATA|FILE_WRITE_DATA|SYNCHRONIZE,&attr,&io,NULL,
        FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_OPEN,
        FILE_NON_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);
    if(!NT_SUCCESS(status)) {GpuHandle=NULL;return status;}
    RtlZeroMemory(&Lease,sizeof(Lease));
    status=SharedLeaseIoctl(GpuHandle,A04_GE8300_IOCTL_SHARED_ACQUIRE,&Lease);
    if(!NT_SUCCESS(status)){ZwClose(GpuHandle);GpuHandle=NULL;return status;}
    if(!Lease.token || !Lease.gpu_adapter || !Lease.gpu_pdo || !Lease.task_sequence || !Lease.flush_sequence)
        status=STATUS_DEVICE_CONFIGURATION_ERROR;
    else status=PrepareShared((PDMA_ADAPTER)Lease.gpu_adapter,(PDEVICE_OBJECT)Lease.gpu_pdo);
    if(NT_SUCCESS(status)) {
        Lease.shared_mdl_owner=&Shared;
        status=SharedLeaseIoctl(GpuHandle,A04_GE8300_IOCTL_SHARED_STAGE,&Lease);
        if(NT_SUCCESS(status) && !Lease.inactive_gpu_pc_dma)status=STATUS_DEVICE_CONFIGURATION_ERROR;
#if A04_GE8300_WDDM_SHARED_DIAGNOSTIC
        if(NT_SUCCESS(status)) {
            /* Non-atomic read-only snapshot; it is neither a frame boundary
             * nor authority to change an enabled OVL layer. */
            Lease.ovl_enable=READ_REGISTER_ULONG(Ovl+0x00c/4);
            Lease.ovl_sources=READ_REGISTER_ULONG(Ovl+0x02c/4);
            Lease.ovl_l0_control=READ_REGISTER_ULONG(Ovl+0x030/4);
            Lease.ovl_l0_size=READ_REGISTER_ULONG(Ovl+0x038/4);
            Lease.ovl_l0_pitch=READ_REGISTER_ULONG(Ovl+0x044/4);
            Lease.ovl_l0_address=READ_REGISTER_ULONG(Ovl+0xf40/4);
            Lease.ovl_interrupt_status=READ_REGISTER_ULONG(Ovl+0x008/4);
            status=SharedLeaseIoctl(GpuHandle,A04_GE8300_IOCTL_SHARED_DIAGNOSTIC,&Lease);
            if(NT_SUCCESS(status) && (!Lease.diagnostic_task || !Lease.diagnostic_flush ||
               Lease.diagnostic_matches!=1024))status=STATUS_DEVICE_CONFIGURATION_ERROR;
        }
#endif
    }
    if(!NT_SUCCESS(status)) {
        NTSTATUS released=SharedLeaseIoctl(GpuHandle,A04_GE8300_IOCTL_SHARED_UNSTAGE,&Lease);
        if(NT_SUCCESS(released))released=a04_shared_scanout_release(&Shared);
        if(NT_SUCCESS(released))released=SharedLeaseIoctl(GpuHandle,A04_GE8300_IOCTL_SHARED_RELEASE,&Lease);
        if(NT_SUCCESS(released)){ZwClose(GpuHandle);GpuHandle=NULL;}
    }
    return status;
}
#endif
VOID A04DisplayResources::Stop()
{
    /* This path may only hold an unexposed shared preparation.  Connecting
     * hardware requires a new stop/removal lifetime, not this cleanup. */
#if A04_GE8300_WDDM_SHARED_LEASE
    if (GpuHandle && Lease.token &&
        !NT_SUCCESS(SharedLeaseIoctl(GpuHandle,A04_GE8300_IOCTL_SHARED_UNSTAGE,&Lease))) return;
#endif
    if (!NT_SUCCESS(a04_shared_scanout_release(&Shared))) return;
#if A04_GE8300_WDDM_SHARED_LEASE
    if (GpuHandle) {
        if (Lease.token && !NT_SUCCESS(SharedLeaseIoctl(GpuHandle,A04_GE8300_IOCTL_SHARED_RELEASE,&Lease))) return;
        ZwClose(GpuHandle);GpuHandle=NULL;
    }
#endif
    if (Ovl) { MmUnmapIoSpace((void *)Ovl,0x1000); Ovl=NULL; }
    if (Adapter && Adapter->DmaOperations && Adapter->DmaOperations->PutDmaAdapter)
        Adapter->DmaOperations->PutDmaAdapter(Adapter);
    Adapter=NULL;
    MapRegisters=0;HasInterrupt=FALSE;DisplayPdo=NULL;
}
#endif
