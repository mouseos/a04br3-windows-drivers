/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_owner_factory_private.h"
#include "native_factory_checkpoint.h"
struct query_wait {KEVENT event;NTSTATUS status;};
static NTSTATUS query_done(PDEVICE_OBJECT device,PIRP irp,void *context)
{
    struct query_wait *w=context;UNREFERENCED_PARAMETER(device);
    w->status=irp->IoStatus.Status;KeSetEvent(&w->event,IO_NO_INCREMENT,FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}
NTSTATUS e_native_platform_open(struct e_native_owner *c)
{
    PWSTR names=NULL;UNICODE_STRING name;PDEVICE_OBJECT device;PIRP irp;
    PIO_STACK_LOCATION stack;struct query_wait wait;NTSTATUS status;size_t length=0;
    if(!c || c->parent_file || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    status=IoGetDeviceInterfaces(&A04SharedGpuDomainGuid,NULL,0,&names);if(!NT_SUCCESS(status))return status;
    if(!names || !names[0]){status=STATUS_NOT_FOUND;goto names_done;}
    while(names[length])length++;
    if(names[length+1]){status=STATUS_OBJECT_NAME_COLLISION;goto names_done;}
    RtlInitUnicodeString(&name,names);
    status=IoGetDeviceObjectPointer(&name,FILE_READ_DATA,&c->parent_file,&device);
    if(!NT_SUCCESS(status))goto names_done;
    irp=IoAllocateIrp(device->StackSize,FALSE);
    if(!irp){status=STATUS_INSUFFICIENT_RESOURCES;goto names_done;}
    KeInitializeEvent(&wait.event,NotificationEvent,FALSE);wait.status=STATUS_NOT_SUPPORTED;
    irp->IoStatus.Status=STATUS_NOT_SUPPORTED;irp->IoStatus.Information=0;
    stack=IoGetNextIrpStackLocation(irp);stack->MajorFunction=IRP_MJ_PNP;stack->MinorFunction=IRP_MN_QUERY_INTERFACE;
    stack->Parameters.QueryInterface.InterfaceType=(LPGUID)&A04SharedGpuDomainGuid;
    stack->Parameters.QueryInterface.Size=sizeof(c->platform);stack->Parameters.QueryInterface.Version=2;
    stack->Parameters.QueryInterface.Interface=(PINTERFACE)&c->platform;
    stack->Parameters.QueryInterface.InterfaceSpecificData=NULL;
    IoSetCompletionRoutine(irp,query_done,&wait,TRUE,TRUE,TRUE);
    status=IoCallDriver(device,irp);
    /* Never timeout/free an IRP or stack completion context still owned by
     * the PnP stack. QUERY_INTERFACE itself performs no GPU execution. */
    (void)status;KeWaitForSingleObject(&wait.event,Executive,KernelMode,FALSE,NULL);
    status=wait.status;IoFreeIrp(irp);
    if(!NT_SUCCESS(status))goto names_done;
    /* Successful QUERY_INTERFACE returns one referenced interface. */
    c->platform_referenced=TRUE;
    if(c->platform.Header.Size!=sizeof(c->platform) || c->platform.Header.Version!=2 ||
       !c->platform.Header.Context || !c->platform.Header.InterfaceReference || !c->platform.Header.InterfaceDereference ||
       !c->platform.Acquire || !c->platform.Observe || !c->platform.Off || !c->platform.On ||
       !c->platform.VerifyOff || !c->platform.ReleaseOff || !c->platform.ReleaseOn){status=STATUS_NOT_SUPPORTED;goto names_done;}
    status=c->platform.Acquire(c->platform.Header.Context,&c->platform_token);
    if(NT_SUCCESS(status)){c->platform_held=TRUE;if(!c->platform_token)status=STATUS_INVALID_DEVICE_STATE;
        else{struct a04_gpu_domain_snapshot snapshot;
            status=e_native_platform_observe(c,&snapshot);
            if(NT_SUCCESS(status) && !(snapshot.spm_status&0x3000010) && !(snapshot.spm_status2&0x3000010)){
                status=c->platform.VerifyOff(c->platform.Header.Context,c->platform_token);
                if(NT_SUCCESS(status))c->hardware_off=TRUE;
            }
        }
    }
names_done:
    if(names)ExFreePool(names);return status;
}
NTSTATUS e_native_platform_observe(struct e_native_owner *c,struct a04_gpu_domain_snapshot *out)
{
    if(!c || !out || !c->platform_held || !c->platform.Observe)return STATUS_INVALID_DEVICE_STATE;
    RtlZeroMemory(out,sizeof(*out));out->size=sizeof(*out);out->version=2;
    NTSTATUS status=c->platform.Observe(c->platform.Header.Context,c->platform_token,out);
    if(NT_SUCCESS(status) && (out->size!=sizeof(*out) || out->version!=2 || out->token!=c->platform_token))return STATUS_INVALID_DEVICE_STATE;
    return status;
}
NTSTATUS e_native_platform_on(struct e_native_owner *c)
{
    struct a04_gpu_domain_snapshot snapshot;NTSTATUS status=e_native_platform_observe(c,&snapshot);
    const ULONG mask=(1u<<25)|(1u<<24)|(1u<<4);
    if(!NT_SUCCESS(status))return status;
    if(!(snapshot.spm_status&mask) && !(snapshot.spm_status2&mask)){
        status=c->platform.On(c->platform.Header.Context,c->platform_token);if(!NT_SUCCESS(status))return status;
        status=e_native_platform_observe(c,&snapshot);if(!NT_SUCCESS(status))return status;
    }
    if((snapshot.spm_status&mask)!=mask || (snapshot.spm_status2&mask)!=mask ||
       (snapshot.protect_ack&0x24) || !(snapshot.way&0x80) || !(snapshot.unlock&1))return STATUS_DEVICE_NOT_READY;
    c->hardware_off=FALSE;c->receipt_valid=FALSE;return STATUS_SUCCESS;
}
NTSTATUS e_native_platform_close(struct e_native_owner *c)
{
    NTSTATUS status;
    if(c->platform_held){
        if(!c->platform.ReleaseOff || !c->platform.ReleaseOn)return STATUS_INVALID_DEVICE_STATE;
        status=c->hardware_off?c->platform.ReleaseOff(c->platform.Header.Context,c->platform_token):
            c->platform.ReleaseOn(c->platform.Header.Context,c->platform_token);
        if(!NT_SUCCESS(status))return status;
        c->platform_held=FALSE;c->platform_token=0;
    }
    if(c->platform_referenced){
        if(!c->platform.Header.InterfaceDereference)return STATUS_INVALID_DEVICE_STATE;
        c->platform.Header.InterfaceDereference(c->platform.Header.Context);c->platform_referenced=FALSE;
    }
    if(c->parent_file){ObDereferenceObject(c->parent_file);c->parent_file=NULL;}
    return STATUS_SUCCESS;
}
NTSTATUS e_native_load_firmware(struct e_native_owner *c)
{
    UNICODE_STRING path=RTL_CONSTANT_STRING(L"\\SystemRoot\\System32\\drivers\\ge8300-mips.fw");
    OBJECT_ATTRIBUTES attributes;IO_STATUS_BLOCK io;FILE_STANDARD_INFORMATION info;HANDLE file=NULL;NTSTATUS status;
    InitializeObjectAttributes(&attributes,&path,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
    status=ZwCreateFile(&file,GENERIC_READ|SYNCHRONIZE,&attributes,&io,NULL,FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ,
        FILE_OPEN,FILE_SYNCHRONOUS_IO_NONALERT|FILE_NON_DIRECTORY_FILE,NULL,0);
    if(!NT_SUCCESS(status))return status;
    status=ZwQueryInformationFile(file,&io,&info,sizeof(info),FileStandardInformation);if(!NT_SUCCESS(status))goto done;
    if(info.EndOfFile.QuadPart<=0 || info.EndOfFile.QuadPart>0x100000 || (info.EndOfFile.QuadPart&4095)){
        status=STATUS_INVALID_IMAGE_FORMAT;goto done;
    }
    c->firmware_bytes=(ULONG)info.EndOfFile.QuadPart;
    c->firmware_image=ExAllocatePoolWithTag(NonPagedPoolNx,c->firmware_bytes,E_FIRMWARE_TAG);
    if(!c->firmware_image){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
    status=ZwReadFile(file,NULL,NULL,NULL,&io,c->firmware_image,c->firmware_bytes,NULL,NULL);
    if(NT_SUCCESS(status) && io.Information!=c->firmware_bytes)status=STATUS_END_OF_FILE;
done:ZwClose(file);return status;
}
NTSTATUS e_native_map_resources(struct e_native_owner *c,PDEVICE_OBJECT pdo,const DXGK_DEVICE_INFO *info)
{
    PHYSICAL_ADDRESS core={0},mfg={0};ULONG irqs=0;
    GeoFactoryCheckpoint(10,STATUS_SUCCESS,pdo?1:0,info?1:0,info && info->PhysicalDeviceObject==pdo,info && info->TranslatedResourceList?info->TranslatedResourceList->Count:0xffffffffu);
    if(!pdo || !info || info->PhysicalDeviceObject!=pdo || !info->TranslatedResourceList ||
       info->TranslatedResourceList->Count!=1)return STATUS_DEVICE_CONFIGURATION_ERROR;
    PCM_PARTIAL_RESOURCE_LIST list=&info->TranslatedResourceList->List[0].PartialResourceList;
    GeoFactoryCheckpoint(11,STATUS_SUCCESS,list->Count,0,0,0);
    if(list->Count>16)return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(ULONG i=0;i<list->Count;i++){
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d=&list->PartialDescriptors[i];
        ULONG x=0,y=0,z=0;
        if(d->Type==CmResourceTypeMemory){x=d->u.Memory.Start.LowPart;y=d->u.Memory.Start.HighPart;z=d->u.Memory.Length;}
        if(d->Type==CmResourceTypeInterrupt){x=d->u.Interrupt.Level;y=d->u.Interrupt.Vector;z=(ULONG)d->u.Interrupt.Affinity;}
        GeoFactoryCheckpoint(12+i,STATUS_SUCCESS,((ULONG)d->Flags<<16)|((ULONG)d->ShareDisposition<<8)|d->Type,x,y,z);
        if(d->ShareDisposition!=CmResourceShareDeviceExclusive)return STATUS_DEVICE_CONFIGURATION_ERROR;
        if(d->Type==CmResourceTypeMemory){
            if(d->u.Memory.Start.QuadPart==0x13000000 && d->u.Memory.Length==0x80000 && !core.QuadPart)core=d->u.Memory.Start;
            else if(d->u.Memory.Start.QuadPart==0x13ffe000 && d->u.Memory.Length==0x1000 && !mfg.QuadPart)mfg=d->u.Memory.Start;
            else return STATUS_DEVICE_CONFIGURATION_ERROR;
        }else if(d->Type==CmResourceTypeInterrupt){
            if(irqs++ || (d->Flags&CM_RESOURCE_INTERRUPT_LATCHED) || d->u.Interrupt.Level<=DISPATCH_LEVEL || d->u.Interrupt.Level>HIGH_LEVEL)
                return STATUS_DEVICE_CONFIGURATION_ERROR;
            c->irq_level=d->u.Interrupt.Level;
        }else return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    if(!core.QuadPart || !mfg.QuadPart || irqs!=1)return STATUS_DEVICE_CONFIGURATION_ERROR;
    c->core=MmMapIoSpace(core,0x80000,MmNonCached);if(!c->core)return STATUS_INSUFFICIENT_RESOURCES;
    c->mfg=MmMapIoSpace(mfg,0x1000,MmNonCached);if(!c->mfg)return STATUS_INSUFFICIENT_RESOURCES;
    return STATUS_SUCCESS;
}
struct sync_call {PKSYNCHRONIZE_ROUTINE routine;void *argument;BOOLEAN called;};
static BOOLEAN sync_run(void *argument)
{struct sync_call *call=argument;call->called=TRUE;return call->routine(call->argument);}
BOOLEAN e_native_synchronize(void *context,PKSYNCHRONIZE_ROUTINE routine,void *argument)
{
    struct e_native_owner *c=context;BOOLEAN value=FALSE;struct sync_call call={routine,argument,FALSE};
    if(!c || !routine || !c->dxgk.DxgkCbSynchronizeExecution || KeGetCurrentIrql()>DISPATCH_LEVEL)return FALSE;
    NTSTATUS status=c->dxgk.DxgkCbSynchronizeExecution(c->dxgk.DeviceHandle,sync_run,&call,0,&value);
    return NT_SUCCESS(status) && call.called && value;
}
