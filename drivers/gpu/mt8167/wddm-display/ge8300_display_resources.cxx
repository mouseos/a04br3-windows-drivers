/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "BDD.hxx"
#include "ge8300_display_resource_check.h"
#include "present_tile.h"
#include "ge8300_trace.hxx"
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
#include "gpu_post_pattern.h"
#endif
#if A04_GE8300_WDDM_GCE_LEASE
#include "../gce/gce_display_interface.h"
struct A04GceDisplayLeaseHolder { A04_GCE_DISPLAY_INTERFACE iface;PFILE_OBJECT file;ULONG token; };
/* Firmware observation is a snapshot, never an ownership or access proof.
 * Report it even if the GCE interface is absent or refuses the lease. */
static void GceTraceFirmwareObservation()
{
    struct Record { ULONG version,bytes,valid,top2,thread_enable,thread_status,thread_irq,ownership; } r;
    UNICODE_STRING name=RTL_CONSTANT_STRING(L"A04BR3GceObservation");
    GUID guid={0xb0f4cc26,0x0ad6,0x48e9,{0x94,0x74,0xe0,0x37,0x2a,0xcf,0x32,0x41}};
    ULONG bytes=sizeof(r),attributes=0;NTSTATUS status;
    RtlZeroMemory(&r,sizeof(r));
    status=ExGetFirmwareEnvironmentVariable(&name,&guid,&r,&bytes,&attributes);
    if(NT_SUCCESS(status) && (bytes!=sizeof(r) || r.version!=1 || r.bytes!=sizeof(r) ||
       r.valid!=1 || r.ownership || r.thread_enable!=MAXULONG ||
       r.thread_status!=MAXULONG || r.thread_irq!=MAXULONG))status=STATUS_DATA_ERROR;
    A04DisplayTrace(0x68,status);
    if(NT_SUCCESS(status))A04DisplayTrace(0x69,(NTSTATUS)r.top2);
    /* Zero means unknown, not idle or secure-world release. */
    A04DisplayTrace(0x6a,(NTSTATUS)0);
}
static NTSTATUS GceQueryCompletion(PDEVICE_OBJECT device,PIRP irp,PVOID context)
{
    UNREFERENCED_PARAMETER(device);UNREFERENCED_PARAMETER(irp);
    KeSetEvent((PKEVENT)context,IO_NO_INCREMENT,FALSE);return STATUS_MORE_PROCESSING_REQUIRED;
}
static NTSTATUS GceDisplayAcquire(void **retained)
{
    PWSTR links=NULL;UNICODE_STRING path;PDEVICE_OBJECT target=NULL;PIRP irp=NULL;
    PIO_STACK_LOCATION stack;KEVENT event;NTSTATUS status;size_t length;
    struct A04GceDisplayLeaseHolder *h=NULL;
    if(!retained || *retained || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    GceTraceFirmwareObservation();
    status=IoGetDeviceInterfaces((LPGUID)&A04GceDeviceInterfaceGuid,NULL,0,&links);
    A04DisplayTrace(0x6b,status);
    if(!NT_SUCCESS(status))return status;
    if(!links || !*links){status=STATUS_NO_SUCH_DEVICE;goto done;}
    RtlInitUnicodeString(&path,links);length=path.Length/sizeof(WCHAR);
    if(links[length+1]){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;}
    h=(struct A04GceDisplayLeaseHolder *)ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*h),'lG4A');
    if(!h){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
    RtlZeroMemory(h,sizeof(*h));
    status=IoGetDeviceObjectPointer(&path,FILE_READ_DATA|FILE_WRITE_DATA,&h->file,&target);
    A04DisplayTrace(0x6c,status);
    if(!NT_SUCCESS(status))goto done;
    irp=IoAllocateIrp(target->StackSize,FALSE);
    if(!irp){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
    KeInitializeEvent(&event,NotificationEvent,FALSE);irp->IoStatus.Status=STATUS_NOT_SUPPORTED;
    stack=IoGetNextIrpStackLocation(irp);stack->MajorFunction=IRP_MJ_PNP;stack->MinorFunction=IRP_MN_QUERY_INTERFACE;
    stack->Parameters.QueryInterface.InterfaceType=&A04GceDisplayInterfaceGuid;
    stack->Parameters.QueryInterface.Size=sizeof(h->iface);
    stack->Parameters.QueryInterface.Version=A04_GCE_DISPLAY_INTERFACE_VERSION;
    stack->Parameters.QueryInterface.Interface=(PINTERFACE)&h->iface;
    stack->Parameters.QueryInterface.InterfaceSpecificData=NULL;
    IoSetCompletionRoutine(irp,GceQueryCompletion,&event,TRUE,TRUE,TRUE);
    status=IoCallDriver(target,irp);
    if(status==STATUS_PENDING) {
        do {status=KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);} while(status!=STATUS_SUCCESS);
    }
    status=irp->IoStatus.Status;IoFreeIrp(irp);irp=NULL;
    A04DisplayTrace(0x6d,status);
    if(!NT_SUCCESS(status))goto done;
    if(!h->iface.Header.InterfaceDereference || !h->iface.Acquire || !h->iface.Release || !h->iface.Ready || !h->iface.ObserveThread0) {
        status=STATUS_NOINTERFACE;goto done;
    }
    status=h->iface.Acquire(h->iface.Header.Context,&h->token);
    A04DisplayTrace(0x6e,status);
    if(h->token && NT_SUCCESS(status)) {
        A04_GCE_THREAD_OBSERVATION o;NTSTATUS observation;
        RtlZeroMemory(&o,sizeof(o));
        observation=h->iface.ObserveThread0(h->iface.Header.Context,h->token,&o);
        A04DisplayTrace(0x70,observation);
        if(NT_SUCCESS(observation)) {
            A04DisplayTrace(0x71,(NTSTATUS)o.enable);A04DisplayTrace(0x72,(NTSTATUS)o.status);
            A04DisplayTrace(0x73,(NTSTATUS)o.irq_status);A04DisplayTrace(0x74,(NTSTATUS)o.irq_enable);
        }
        status=h->iface.Ready(h->iface.Header.Context,h->token);
        A04DisplayTrace(0x6f,status);
        if(!NT_SUCCESS(status) && NT_SUCCESS(h->iface.Release(h->iface.Header.Context,h->token)))h->token=0;
    }
    if(h->token){*retained=h;h=NULL;goto done;} /* retain even on failure */
    if(NT_SUCCESS(status))status=STATUS_DEVICE_CONFIGURATION_ERROR;
done:
    if(irp)IoFreeIrp(irp);
    if(h){if(h->iface.Header.InterfaceDereference)h->iface.Header.InterfaceDereference(h->iface.Header.Context);
          if(h->file)ObDereferenceObject(h->file);ExFreePoolWithTag(h,'lG4A');}
    if(links)ExFreePool(links);return status;
}
#endif
#if A04_GE8300_WDDM_DIRECT_EXPERIMENT
NTSTATUS A04DisplayResources::Start(PDEVICE_OBJECT pdo,const DXGK_DEVICE_INFO *info)
{
    PCM_RESOURCE_LIST list; PCM_PARTIAL_RESOURCE_LIST partial; PCM_FULL_RESOURCE_DESCRIPTOR full;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR ovl=NULL,irq=NULL;
    ULONG matches=0,interrupts=0,i,j,flags=0;DEVICE_DESCRIPTION desc;
    /* Trace raw PnP metadata, never a kernel pointer or an MMIO read. */
    flags=(pdo?1u:0u)|(info?2u:0u)|(info && info->PhysicalDeviceObject==pdo?4u:0u)|
        (Adapter?8u:0u)|(Ovl?16u:0u)|((ULONG)KeGetCurrentIrql()<<8);
    A04DisplayTrace(0x40,(NTSTATUS)flags);
    if (!pdo || !info || info->PhysicalDeviceObject!=pdo || Adapter || Ovl ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    list=info->TranslatedResourceList;
    A04DisplayTrace(0x41,(NTSTATUS)(list?list->Count:0xffffffffu));
    if (!list || !list->Count || list->Count>16) return STATUS_DEVICE_CONFIGURATION_ERROR;
    full=&list->List[0];
    for(j=0;j<list->Count;j++) {
        partial=&full->PartialResourceList;
        A04DisplayTrace(0x42,(NTSTATUS)((j<<16)|(partial->Count&0xffffu)));
        if(partial->Count>128)return STATUS_DEVICE_CONFIGURATION_ERROR;
        for(i=0;i<partial->Count;i++) {
            PCM_PARTIAL_RESOURCE_DESCRIPTOR item=&partial->PartialDescriptors[i];
            if(i<16u) {
                A04DisplayTrace(0x43,(NTSTATUS)((j<<24)|(i<<16)|((ULONG)item->ShareDisposition<<8)|item->Type));
                if(item->Type==CmResourceTypeMemory) {
                    A04DisplayTrace(0x44,(NTSTATUS)item->u.Memory.Start.LowPart);
                    A04DisplayTrace(0x45,(NTSTATUS)item->u.Memory.Start.HighPart);
                    A04DisplayTrace(0x46,(NTSTATUS)item->u.Memory.Length);
                }
            }
            if (item->Type==CmResourceTypeMemory &&
                (ULONGLONG)item->u.Memory.Start.QuadPart==0x14007000ULL) { ovl=item; matches++; }
            if (item->Type==CmResourceTypeInterrupt) { irq=item; interrupts++; }
        }
        full=(PCM_FULL_RESOURCE_DESCRIPTOR)(void *)&partial->PartialDescriptors[partial->Count];
    }
    flags=(matches&0xffu)|((interrupts&0xffu)<<8)|
        (ovl && ovl->ShareDisposition==CmResourceShareDeviceExclusive?0x10000u:0u)|
        (irq && irq->ShareDisposition==CmResourceShareDeviceExclusive?0x20000u:0u);
    A04DisplayTrace(0x47,(NTSTATUS)flags);
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
    /* The POST diagnostic has no display IRQ handler. Do not start its
     * resource/worker path with an interrupt-bearing PDO. */
    if(interrupts) {A04DisplayTrace(0x8a,STATUS_DEVICE_CONFIGURATION_ERROR);return STATUS_DEVICE_CONFIGURATION_ERROR;}
#endif
    if (!ovl || !a04_display_resources_valid(matches,interrupts,
        ovl->ShareDisposition==CmResourceShareDeviceExclusive,
        irq && irq->ShareDisposition==CmResourceShareDeviceExclusive,
        (uint64_t)ovl->u.Memory.Start.QuadPart,ovl->u.Memory.Length))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    /* OVL has one linear layer address, not a hardware SG descriptor list.
     * GetScatterGatherList still works for a non-SG bus master: Windows must
     * provide an appropriate contiguous mapping or reject the request. */
    RtlZeroMemory(&desc,sizeof(desc));desc.Version=DEVICE_DESCRIPTION_VERSION3;
    desc.Master=TRUE;desc.ScatterGather=FALSE;desc.DmaAddressWidth=32;
    desc.InterfaceType=Internal;desc.MaximumLength=4096000;
    Adapter=IoGetDmaAdapter(pdo,&desc,&MapRegisters);
    flags=(Adapter?1u:0u)|(Adapter && Adapter->DmaOperations?2u:0u);
    if(Adapter && Adapter->DmaOperations && Adapter->DmaOperations->Size>=
        FIELD_OFFSET(DMA_OPERATIONS,PutScatterGatherList)+sizeof(Adapter->DmaOperations->PutScatterGatherList)) {
        flags|=4u;
        if(Adapter->DmaOperations->GetScatterGatherList)flags|=8u;
        if(Adapter->DmaOperations->PutScatterGatherList)flags|=16u;
        if(Adapter->DmaOperations->PutDmaAdapter)flags|=32u;
    }
    A04DisplayTrace(0x48,(NTSTATUS)flags);
    A04DisplayTrace(0x49,(NTSTATUS)MapRegisters);
    if ((flags&63u)!=63u || MapRegisters<
#if A04_GE8300_WDDM_SHARED_COMMON_PAGE
        1u
#else
        1000u
#endif
        ) { Stop(); return STATUS_NOT_SUPPORTED; }
    Ovl=(volatile ULONG *)MmMapIoSpace(ovl->u.Memory.Start,0x1000,MmNonCached);
    A04DisplayTrace(0x4a,Ovl?STATUS_SUCCESS:STATUS_INSUFFICIENT_RESOURCES);
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
#if A04_GE8300_WDDM_SHARED_COMMON_PAGE
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
static NTSTATUS CommonFrameCapabilities(HANDLE handle)
{
    struct a04_ge8300_shared_capabilities caps={};IO_STATUS_BLOCK io={};PFILE_OBJECT file=NULL;NTSTATUS status;
    status=ObReferenceObjectByHandle(handle,SYNCHRONIZE,*IoFileObjectType,KernelMode,(PVOID *)&file,NULL);
    if(!NT_SUCCESS(status))return status;
    status=ZwDeviceIoControlFile(handle,NULL,NULL,NULL,&io,A04_GE8300_IOCTL_SHARED_CAPABILITIES,
        &caps,sizeof(caps),&caps,sizeof(caps));
    if(status==STATUS_PENDING) {
        do {status=KeWaitForSingleObject(file,Executive,KernelMode,FALSE,NULL);} while(status!=STATUS_SUCCESS);
        status=io.Status;
    } else if(NT_SUCCESS(status))status=io.Status;
    ObDereferenceObject(file);
    if(NT_SUCCESS(status) && (io.Information!=sizeof(caps) || caps.version!=1 || caps.bytes!=sizeof(caps) ||
       !(caps.flags&A04_GE8300_SHARED_CAP_FRAME_SEAL) || caps.reserved))return STATUS_DEVICE_CONFIGURATION_ERROR;
    return status;
}
#endif
static NTSTATUS CommonPageIoctl(HANDLE handle,ULONG code,struct a04_ge8300_common_page *page)
{
    IO_STATUS_BLOCK io={};NTSTATUS status;PFILE_OBJECT file=NULL;
    status=ObReferenceObjectByHandle(handle,SYNCHRONIZE,*IoFileObjectType,KernelMode,(PVOID *)&file,NULL);
    if(!NT_SUCCESS(status))return status;
    status=ZwDeviceIoControlFile(handle,NULL,NULL,NULL,&io,code,
        page,sizeof(*page),page,sizeof(*page));
    if(status==STATUS_PENDING) {
        do {status=KeWaitForSingleObject(file,Executive,KernelMode,FALSE,NULL);} while(status!=STATUS_SUCCESS);
        status=io.Status;
    } else if(NT_SUCCESS(status))status=io.Status;
    ObDereferenceObject(file);
    if(NT_SUCCESS(status) && io.Information!=sizeof(*page))return STATUS_IO_DEVICE_ERROR;
    return status;
}
NTSTATUS A04DisplayResources::ProbeSharedCommonPage()
{
    struct a04_ge8300_common_page page={};PMDL mdl=NULL;PVOID dmaContext=NULL;
    PSCATTER_GATHER_LIST sg=NULL;NTSTATUS status,release;BOOLEAN mapped=FALSE;
    if(!a04_common_claim_attempt(&SharedCommonOnce))return STATUS_DEVICE_BUSY;
    if(!GpuHandle || !Adapter || !DisplayPdo || !Adapter->DmaOperations ||
       Adapter->DmaOperations->Size<FIELD_OFFSET(DMA_OPERATIONS,FreeAdapterObject)+sizeof(Adapter->DmaOperations->FreeAdapterObject) ||
       !Adapter->DmaOperations->InitializeDmaTransferContext || !Adapter->DmaOperations->GetScatterGatherListEx ||
       !Adapter->DmaOperations->FreeAdapterObject)return STATUS_NOT_SUPPORTED;
    A04DisplayTrace(0x87,STATUS_PENDING);
    status=CommonPageIoctl(GpuHandle,A04_GE8300_IOCTL_SHARED_COMMON_PAGE,&page);A04DisplayTrace(0x50,status);
    if(!NT_SUCCESS(status)) {
        if(page.token) {
            release=CommonPageIoctl(GpuHandle,A04_GE8300_IOCTL_SHARED_COMMON_PAGE,&page);A04DisplayTrace(0x56,release);
            if(!NT_SUCCESS(release))SharedCommonRetained=TRUE;
        }
        return status;
    }
    if(!page.token || !page.cpu || ((ULONG_PTR)page.cpu&4095u) || !page.gpu_va || !page.task || !page.flush ||
       !page.gpu_adapter || !page.gpu_pdo || page.gpu_adapter==Adapter || page.gpu_pdo==DisplayPdo) {
        status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;
    }
    A04DisplayTrace(0x54,(NTSTATUS)page.task);A04DisplayTrace(0x55,(NTSTATUS)page.flush);
    mdl=IoAllocateMdl(page.cpu,4096,FALSE,FALSE,NULL);
    dmaContext=ExAllocatePoolWithTag(NonPagedPoolNx,DMA_TRANSFER_CONTEXT_SIZE_V1,'mG4A');
    if(!mdl || !dmaContext){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
    MmBuildMdlForNonPagedPool(mdl);
    status=Adapter->DmaOperations->InitializeDmaTransferContext(Adapter,dmaContext);
    if(!NT_SUCCESS(status))goto done;
    /* Synchronous V3 mapping cannot queue indefinitely when registers are
     * exhausted. No display transfer is initiated in this diagnostic. */
    status=Adapter->DmaOperations->GetScatterGatherListEx(Adapter,DisplayPdo,dmaContext,mdl,0,4096,
        DMA_SYNCHRONOUS_CALLBACK,NULL,NULL,TRUE,NULL,NULL,&sg);
    A04DisplayTrace(0x51,status);
    if(status!=STATUS_SUCCESS) { if(NT_SUCCESS(status))status=STATUS_DEVICE_NOT_READY;goto done; }
    mapped=TRUE;
    if(!sg || sg->NumberOfElements!=1 || sg->Elements[0].Length!=4096u ||
       sg->Elements[0].Address.QuadPart<=0 || sg->Elements[0].Address.QuadPart>0xfffff000LL ||
       (sg->Elements[0].Address.LowPart&4095u)) status=STATUS_DEVICE_CONFIGURATION_ERROR;
    else {A04DisplayTrace(0x52,(NTSTATUS)sg->NumberOfElements);
          A04DisplayTrace(0x53,(NTSTATUS)sg->Elements[0].Address.LowPart);}
    /* Releasing the mapping cannot prove that the display can see GPU data. */
done:
    if(mapped)Adapter->DmaOperations->FreeAdapterObject(Adapter,DeallocateObject);
    if(dmaContext)ExFreePoolWithTag(dmaContext,'mG4A');
    if(mdl)IoFreeMdl(mdl);
#if A04_GE8300_WDDM_SHARED_FULL_COMMON
    if(NT_SUCCESS(status))status=ProbeDisplayFullCommon(&page);
#endif
    if(SharedCommonRetained)return status;
    release=CommonPageIoctl(GpuHandle,A04_GE8300_IOCTL_SHARED_COMMON_PAGE,&page);A04DisplayTrace(0x56,release);
    if(!NT_SUCCESS(release)){SharedCommonRetained=TRUE;return release;}
    return status;
}
#endif
#if A04_GE8300_WDDM_SHARED_FULL_COMMON
#if A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC
/* Independent of the display object: once the provider can see the root,
 * retain this object, both adapter mappings and both PDO references. */
struct A04CommonFrameOwner {
    struct a04_shared_scanout shared;
    PVOID cpu,context;
    PDEVICE_OBJECT displayPdo;
    HANDLE gpuHandle;
    uint32_t frame,task,flush,cpuMatches;
    BOOLEAN sealed;
#if A04_GE8300_WDDM_EXPLICIT_GPU_FLUSH
    PVOID mapRegisterBase;
#endif
};
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
NTSTATUS A04DisplayResources::GetVerifiedGpuFrame(const uint32_t **pixels)
{
    A04CommonFrameOwner *owner=(A04CommonFrameOwner *)SharedCommonOwner;
    if(!pixels)return STATUS_INVALID_PARAMETER;
    *pixels=NULL;
    if(!SharedCommonRetained || !owner || !owner->cpu || !owner->sealed ||
       !owner->task || !owner->flush || owner->cpuMatches!=1024000u)
        return STATUS_DEVICE_NOT_READY;
    *pixels=(const uint32_t *)owner->cpu;return STATUS_SUCCESS;
}
#endif
static uint32_t CommonFramePixel(uint32_t x,uint32_t y)
{
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
    return a04_gpu_post_pixel(x,y);
#else
    return (y*800u+x)^0x6d3a59c7u;
#endif
}
static NTSTATUS CommonFrameTiles(HANDLE handle,uint32_t token,PMDL mdl,volatile uint32_t *cpu,
    struct A04CommonFrameOwner *owner)
{
    struct a04_ge8300_shared_tile *tile;IO_STATUS_BLOCK io={};PFILE_OBJECT file=NULL;
    NTSTATUS status;uint32_t index=0,matches=0,lastTask=0,lastFlush=0;
    tile=(struct a04_ge8300_shared_tile *)ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*tile),'fG4A');
    if(!tile){A04DisplayTrace(0x5e,0);A04DisplayTrace(0x5f,STATUS_INSUFFICIENT_RESOURCES);return STATUS_INSUFFICIENT_RESOURCES;}
    status=ObReferenceObjectByHandle(handle,SYNCHRONIZE,*IoFileObjectType,KernelMode,(PVOID *)&file,NULL);
    if(!NT_SUCCESS(status)){A04DisplayTrace(0x5e,0);A04DisplayTrace(0x5f,status);goto done;}
    for(index=0;index<1000u;index++) {
        uint32_t x=(index%25u)*32u,y=(index/25u)*32u;
        RtlZeroMemory(tile,sizeof(*tile));tile->token=token;tile->frame=1;tile->index=index;
        for(uint32_t row=0;row<32u;row++)for(uint32_t col=0;col<32u;col++)
            tile->pixels[row*32u+col]=CommonFramePixel(x+col,y+row);
        io.Information=0;io.Status=STATUS_PENDING;
        status=ZwDeviceIoControlFile(handle,NULL,NULL,NULL,&io,A04_GE8300_IOCTL_SHARED_TILE,
            tile,sizeof(*tile),tile,sizeof(*tile));
        if(status==STATUS_PENDING) {
            do {status=KeWaitForSingleObject(file,Executive,KernelMode,FALSE,NULL);} while(status!=STATUS_SUCCESS);
            status=io.Status;
        } else if(NT_SUCCESS(status))status=io.Status;
        if(!NT_SUCCESS(status) || io.Information!=sizeof(*tile) || !tile->task || !tile->flush) {
            if(NT_SUCCESS(status))status=STATUS_IO_DEVICE_ERROR;break;
        }
        lastTask=tile->task;lastFlush=tile->flush;
    }
    A04DisplayTrace(0x5e,(NTSTATUS)index);A04DisplayTrace(0x5f,status);
    A04DisplayTrace(0x62,(NTSTATUS)lastTask);A04DisplayTrace(0x63,(NTSTATUS)lastFlush);
    if(NT_SUCCESS(status) && index==1000u) {
        struct a04_ge8300_frame_seal seal={token,1,lastTask,lastFlush};
        io.Information=0;io.Status=STATUS_PENDING;
        status=ZwDeviceIoControlFile(handle,NULL,NULL,NULL,&io,A04_GE8300_IOCTL_SHARED_FRAME_SEAL,
            &seal,sizeof(seal),&seal,sizeof(seal));
        if(status==STATUS_PENDING) {
            do {status=KeWaitForSingleObject(file,Executive,KernelMode,FALSE,NULL);} while(status!=STATUS_SUCCESS);
            status=io.Status;
        } else if(NT_SUCCESS(status))status=io.Status;
        if(NT_SUCCESS(status) && io.Information!=sizeof(seal))status=STATUS_IO_DEVICE_ERROR;
        A04DisplayTrace(0x66,status);
        if(!NT_SUCCESS(status))goto done;
        owner->frame=1;owner->task=lastTask;owner->flush=lastFlush;owner->sealed=TRUE;
        /* This is a CPU observation, not a display-adapter visibility proof.
         * A GPU DMA bounce mapping can cause a mismatch. */
#if A04_GE8300_WDDM_EXPLICIT_GPU_FLUSH
        /* Full GPU transfer is finished and sealed. Flush the exact V3 map
         * before the CPU reads it; this does not retire the published root. */
        status=owner->shared.gpu_adapter->DmaOperations->FlushAdapterBuffersEx(
            owner->shared.gpu_adapter,mdl,owner->mapRegisterBase,0,4096000u,FALSE);
        A04DisplayTrace(0x98,status);
        if(!NT_SUCCESS(status))goto done;
#endif
        KeFlushIoBuffers(mdl,TRUE,TRUE);KeMemoryBarrier();
        for(uint32_t y=0;y<1280u;y++)for(uint32_t x=0;x<800u;x++)
            if(cpu[y*800u+x]==CommonFramePixel(x,y))matches++;
        status=matches==1024000u?STATUS_SUCCESS:STATUS_DATA_ERROR;
        owner->cpuMatches=matches;
        A04DisplayTrace(0x60,(NTSTATUS)matches);A04DisplayTrace(0x61,status);
    }
done:
    if(file)ObDereferenceObject(file);ExFreePoolWithTag(tile,'fG4A');return status;
}
#endif
NTSTATUS A04DisplayResources::ProbeDisplayFullCommon(struct a04_ge8300_common_page *page)
{
    PDMA_ADAPTER gpu=(PDMA_ADAPTER)page->gpu_adapter;PDEVICE_OBJECT gpuPdo=(PDEVICE_OBJECT)page->gpu_pdo;
    PHYSICAL_ADDRESS max,logical;PVOID cpu=NULL,context=NULL;PMDL mdl=NULL;
    PSCATTER_GATHER_LIST sg=NULL;NTSTATUS status=STATUS_NOT_SUPPORTED;BOOLEAN mapped=FALSE;
#if A04_GE8300_WDDM_EXPLICIT_GPU_FLUSH
    DMA_TRANSFER_INFO transferInfo;PVOID mapRegisterBase=NULL;BOOLEAN channelAllocated=FALSE;
#endif
    uint64_t total=0;ULONG i,j;
#if A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC
    struct A04CommonFrameOwner *owner=NULL;uint32_t npage=0;BOOLEAN layoutValid=FALSE;
#endif
    if(!Adapter || !gpu || !gpuPdo || gpu==Adapter || !Adapter->DmaOperations || !gpu->DmaOperations ||
       Adapter->DmaOperations->Size<FIELD_OFFSET(DMA_OPERATIONS,AllocateCommonBufferEx)+sizeof(Adapter->DmaOperations->AllocateCommonBufferEx) ||
       !Adapter->DmaOperations->AllocateCommonBufferEx || !Adapter->DmaOperations->FreeCommonBuffer) goto done;
    max.QuadPart=0xffffffffLL;logical.QuadPart=0;
    cpu=Adapter->DmaOperations->AllocateCommonBufferEx(Adapter,&max,4096000u,&logical,FALSE,0);
    if(!cpu){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
    if(!a04_display_common_range_valid((uint64_t)logical.QuadPart,(uintptr_t)cpu,4096000u)) {
        status=STATUS_DEVICE_CONFIGURATION_ERROR;A04DisplayTrace(0x57,status);goto done;
    }
    A04DisplayTrace(0x57,STATUS_SUCCESS);A04DisplayTrace(0x58,(NTSTATUS)logical.LowPart);
    A04DisplayTrace(0x59,(NTSTATUS)4096000u);
#if A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC
    for(i=0;i<1024000u;i++)((volatile uint32_t *)cpu)[i]=~CommonFramePixel(i%800u,i/800u);
    KeMemoryBarrier();
#endif
    if(gpu->DmaOperations->Size<FIELD_OFFSET(DMA_OPERATIONS,FreeAdapterObject)+sizeof(gpu->DmaOperations->FreeAdapterObject) ||
       !gpu->DmaOperations->InitializeDmaTransferContext || !gpu->DmaOperations->GetScatterGatherListEx ||
       !gpu->DmaOperations->FreeAdapterObject){status=STATUS_NOT_SUPPORTED;goto gpu_done;}
    mdl=IoAllocateMdl(cpu,4096000u,FALSE,FALSE,NULL);
    context=ExAllocatePoolWithTag(NonPagedPoolNx,DMA_TRANSFER_CONTEXT_SIZE_V1,'xG4A');
    if(!mdl || !context){status=STATUS_INSUFFICIENT_RESOURCES;goto gpu_done;}
    MmBuildMdlForNonPagedPool(mdl);
    status=gpu->DmaOperations->InitializeDmaTransferContext(gpu,context);
    if(!NT_SUCCESS(status))goto gpu_done;
    /* One synchronous request: no queued partial maps, no fabric access. */
#if A04_GE8300_WDDM_EXPLICIT_GPU_FLUSH
    if(gpu->DmaOperations->Size<FIELD_OFFSET(DMA_OPERATIONS,FlushAdapterBuffersEx)+sizeof(gpu->DmaOperations->FlushAdapterBuffersEx) ||
       !gpu->DmaOperations->GetDmaTransferInfo || !gpu->DmaOperations->AllocateAdapterChannelEx ||
       !gpu->DmaOperations->MapTransferEx || !gpu->DmaOperations->FlushAdapterBuffersEx) {status=STATUS_NOT_SUPPORTED;goto gpu_done;}
    RtlZeroMemory(&transferInfo,sizeof(transferInfo));transferInfo.Version=DMA_TRANSFER_INFO_VERSION1;
    status=gpu->DmaOperations->GetDmaTransferInfo(gpu,mdl,0,4096000u,FALSE,&transferInfo);
    if(!NT_SUCCESS(status))goto gpu_done;
    if(!transferInfo.V1.MapRegisterCount || transferInfo.V1.MapRegisterCount>1001u ||
       transferInfo.V1.ScatterGatherListSize<sizeof(SCATTER_GATHER_LIST) ||
       transferInfo.V1.ScatterGatherListSize>1024u*1024u) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto gpu_done;}
    sg=(PSCATTER_GATHER_LIST)ExAllocatePoolWithTag(NonPagedPoolNx,transferInfo.V1.ScatterGatherListSize,'sG4A');
    if(!sg){status=STATUS_INSUFFICIENT_RESOURCES;goto gpu_done;}
    RtlZeroMemory(sg,transferInfo.V1.ScatterGatherListSize);
    status=gpu->DmaOperations->AllocateAdapterChannelEx(gpu,gpuPdo,context,
        transferInfo.V1.MapRegisterCount,DMA_SYNCHRONOUS_CALLBACK,NULL,NULL,&mapRegisterBase);
    if(status!=STATUS_SUCCESS){if(NT_SUCCESS(status))status=STATUS_DEVICE_NOT_READY;goto gpu_done;}
    channelAllocated=TRUE;
    if(!mapRegisterBase){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto gpu_done;}
    {ULONG length=4096000u;
     status=gpu->DmaOperations->MapTransferEx(gpu,mdl,mapRegisterBase,0,0,&length,FALSE,sg,
         transferInfo.V1.ScatterGatherListSize,NULL,NULL);
     if(status!=STATUS_SUCCESS || length!=4096000u) {if(NT_SUCCESS(status))status=STATUS_DEVICE_CONFIGURATION_ERROR;goto gpu_done;}}
    mapped=TRUE;
#else
    status=gpu->DmaOperations->GetScatterGatherListEx(gpu,gpuPdo,context,mdl,0,4096000u,
        DMA_SYNCHRONOUS_CALLBACK,NULL,NULL,FALSE,NULL,NULL,&sg);
    if(status!=STATUS_SUCCESS){if(NT_SUCCESS(status))status=STATUS_DEVICE_NOT_READY;goto gpu_done;}
    mapped=TRUE;
#endif
    if(!sg || !sg->NumberOfElements || sg->NumberOfElements>1000u){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto gpu_done;}
    for(i=0;i<sg->NumberOfElements;i++) {
        uint64_t a=(uint64_t)sg->Elements[i].Address.QuadPart;uint32_t n=sg->Elements[i].Length;
        if(!n || (a&4095u) || (n&4095u) || a>=(UINT64_C(1)<<40) ||
           n>(UINT64_C(1)<<40)-a || n>4096000u-total) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto gpu_done;}
        for(j=0;j<i;j++) {
            uint64_t other=(uint64_t)sg->Elements[j].Address.QuadPart;
            if(a<other+sg->Elements[j].Length && other<a+n){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto gpu_done;}
        }
        total+=n;
    }
    if(total!=4096000u){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto gpu_done;}
    A04DisplayTrace(0x5c,(NTSTATUS)sg->NumberOfElements);
#if A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC
    A04DisplayTrace(0x5b,STATUS_SUCCESS);
    layoutValid=TRUE;
    owner=(struct A04CommonFrameOwner *)ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*owner),'oG4A');
    if(!owner){status=STATUS_INSUFFICIENT_RESOURCES;goto gpu_done;}
    RtlZeroMemory(owner,sizeof(*owner));
    owner->shared.gpu_pages=(uint64_t *)ExAllocatePoolWithTag(NonPagedPoolNx,1000u*sizeof(uint64_t),'pG4A');
    if(!owner->shared.gpu_pages){status=STATUS_INSUFFICIENT_RESOURCES;goto gpu_done;}
    for(i=0;i<sg->NumberOfElements;i++)for(j=0;j<sg->Elements[i].Length;j+=4096u)
        owner->shared.gpu_pages[npage++]=(uint64_t)sg->Elements[i].Address.QuadPart+j;
    if(npage!=1000u){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto gpu_done;}
    owner->shared.mdl=mdl;owner->shared.gpu_adapter=gpu;owner->shared.display_adapter=Adapter;
    owner->shared.gpu_sg=sg;owner->shared.display_dma=logical.LowPart;owner->shared.display_common=TRUE;
    owner->cpu=cpu;owner->context=context;owner->displayPdo=DisplayPdo;owner->gpuHandle=GpuHandle;
#if A04_GE8300_WDDM_EXPLICIT_GPU_FLUSH
    owner->mapRegisterBase=mapRegisterBase;
#endif
    status=CommonPageIoctl(GpuHandle,A04_GE8300_IOCTL_SHARED_COMMON_PROMOTE,page);
    A04DisplayTrace(0x64,status);
    if(!NT_SUCCESS(status))goto gpu_done;
    ObReferenceObject(DisplayPdo);
    SharedCommonRetained=TRUE;SharedCommonOwner=owner;
    RtlZeroMemory(&Lease,sizeof(Lease));Lease.token=page->token;
    Lease.gpu_adapter=gpu;Lease.gpu_pdo=gpuPdo;Lease.shared_mdl_owner=&owner->shared;
    status=SharedLeaseIoctl(GpuHandle,A04_GE8300_IOCTL_SHARED_STAGE,&Lease);
    A04DisplayTrace(0x5d,status);
    if(NT_SUCCESS(status))status=CommonFrameTiles(GpuHandle,Lease.token,mdl,(volatile uint32_t *)cpu,owner);
#if A04_GE8300_WDDM_GCE_LEASE
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
    if(NT_SUCCESS(status)) {NTSTATUS gceStatus=GceDisplayAcquire(&GceDisplayLease);A04DisplayTrace(0x67,gceStatus);}
#else
    if(NT_SUCCESS(status)) {status=GceDisplayAcquire(&GceDisplayLease);A04DisplayTrace(0x67,status);}
#endif
#endif
    /* Read-only, non-atomic snapshot; no acknowledgement, IRQ clearing or
     * timing inference.  Firmware can enable both GOP L0 and overlay L3. */
    if(Ovl) {
        A04DisplayTrace(0x70,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x00c/4));
        A04DisplayTrace(0x71,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x02c/4));
        A04DisplayTrace(0x72,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x020/4));
        A04DisplayTrace(0x73,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x030/4));
        A04DisplayTrace(0x74,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x038/4));
        A04DisplayTrace(0x75,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x044/4));
        A04DisplayTrace(0x76,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0xf40/4));
        A04DisplayTrace(0x77,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x090/4));
        A04DisplayTrace(0x78,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x098/4));
        A04DisplayTrace(0x79,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x0a4/4));
        A04DisplayTrace(0x7a,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0xfa0/4));
        A04DisplayTrace(0x7b,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x004/4));
        A04DisplayTrace(0x7c,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x008/4));
#if A04_GE8300_WDDM_OVL_OBSERVE
        /* Linux mt8167 ddp_ovl.c reads these debug registers without clearing
         * IRQs. Samples are non-atomic and never authorize a flip. */
        for(ULONG sample=0;sample<2u;sample++) {
            if(sample) { LARGE_INTEGER delay;delay.QuadPart=-200000LL;
                (void)KeDelayExecutionThread(KernelMode,FALSE,&delay); }
            ULONG base=0x90u+sample*4u;
            A04DisplayTrace(base,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x240/4));
            A04DisplayTrace(base+1,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x244/4));
            A04DisplayTrace(base+2,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x24c/4));
            A04DisplayTrace(base+3,(NTSTATUS)READ_REGISTER_ULONG(Ovl+0x258/4));
        }
#endif
    }
    /* Even failed stage may have published or aliased a root: never unwind. */
    return status;
#else
    /* A cross-domain map is not evidence that either device accessed it.
     * Do not publish a mapping without proven post-publication retirement. */
    A04DisplayTrace(0x5b,STATUS_SUCCESS);
    status=STATUS_NOT_SUPPORTED;A04DisplayTrace(0x5d,status);
    goto cleanup_gpu;
#endif
gpu_done:
#if A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC
    A04DisplayTrace(layoutValid?0x65:0x5b,status);
#else
    A04DisplayTrace(0x5b,status);
#endif
#if A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC
    if(owner){if(owner->shared.gpu_pages)ExFreePoolWithTag(owner->shared.gpu_pages,'pG4A');ExFreePoolWithTag(owner,'oG4A');}
#endif
cleanup_gpu:
#if A04_GE8300_WDDM_EXPLICIT_GPU_FLUSH
    if(channelAllocated)gpu->DmaOperations->FreeAdapterObject(gpu,DeallocateObject);
    if(sg)ExFreePoolWithTag(sg,'sG4A');
#else
    if(mapped)gpu->DmaOperations->FreeAdapterObject(gpu,DeallocateObject);
#endif
    if(context)ExFreePoolWithTag(context,'xG4A');
    if(mdl)IoFreeMdl(mdl);
done:
    if(!cpu)A04DisplayTrace(0x57,status);
    else {
        Adapter->DmaOperations->FreeCommonBuffer(Adapter,4096000u,logical,cpu,FALSE);
        A04DisplayTrace(0x5a,STATUS_SUCCESS);
    }
    return status;
}

#endif
NTSTATUS A04DisplayResources::PrepareViaGpu()
{
    UNICODE_STRING name=RTL_CONSTANT_STRING(L"\\Device\\A04GE8300");
    OBJECT_ATTRIBUTES attr;IO_STATUS_BLOCK io={};NTSTATUS status;
    if(!Adapter || !Ovl || !DisplayPdo || GpuHandle || Shared.mdl ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL || KeAreAllApcsDisabled())return STATUS_DEVICE_NOT_READY;
#if A04_GE8300_WDDM_SHARED_COMMON_PAGE
    if(!a04_common_may_attempt(&SharedCommonOnce))return STATUS_DEVICE_BUSY;
#endif
    InitializeObjectAttributes(&attr,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
    A04DisplayTrace(0x85,STATUS_PENDING);
    status=ZwCreateFile(&GpuHandle,FILE_READ_DATA|FILE_WRITE_DATA|SYNCHRONIZE,&attr,&io,NULL,
        FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_OPEN,
        FILE_NON_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);
    A04DisplayTrace(0x86,status);
    if(!NT_SUCCESS(status)) {GpuHandle=NULL;return status;}
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
    status=CommonFrameCapabilities(GpuHandle);A04DisplayTrace(0x8b,status);
    if(!NT_SUCCESS(status)){
        (void)a04_common_claim_attempt(&SharedCommonOnce);
        SharedCommonFinished=TRUE;SharedCommonResult=status;
        ZwClose(GpuHandle);GpuHandle=NULL;return status;
    }
#endif
#if A04_GE8300_WDDM_SHARED_COMMON_PAGE
    status=ProbeSharedCommonPage();
    SharedCommonResult=status;SharedCommonFinished=TRUE;
    /* A failed release leaves the provider's referenced lease alive. */
    if(!SharedCommonRetained){ZwClose(GpuHandle);GpuHandle=NULL;}
    return status;
#endif
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
#if A04_GE8300_WDDM_SHARED_COMMON_PAGE
    if(SharedCommonRetained)return;
#endif
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
    if (Adapter && Adapter->DmaOperations && Adapter->DmaOperations->Size>=
        FIELD_OFFSET(DMA_OPERATIONS,PutDmaAdapter)+sizeof(Adapter->DmaOperations->PutDmaAdapter) &&
        Adapter->DmaOperations->PutDmaAdapter)
        Adapter->DmaOperations->PutDmaAdapter(Adapter);
    Adapter=NULL;
    MapRegisters=0;HasInterrupt=FALSE;DisplayPdo=NULL;
}
#endif
