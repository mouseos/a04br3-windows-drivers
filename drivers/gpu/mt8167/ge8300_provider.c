/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * GE8300 PnP firmware bring-up. Disabled by default. The controlled WinPE
 * build starts, health-checks and shuts the firmware down once. */
#include <ntddk.h>
#include <wdf.h>
#include "windows_firmware.h"
#include "ge8300_status.h"
#ifndef A04_GE8300_BOOT_TEST
#define A04_GE8300_BOOT_TEST 0
#endif
#ifndef A04_GE8300_ALLOW_MISMATCH
#define A04_GE8300_ALLOW_MISMATCH 0
#endif
#define CORE_BASE 0x13000000u
#define CORE_SIZE 0x80000u
#define MFG_BASE  0x13ffe000u
#define MFG_SIZE  0x1000u
#define MAX_FIRMWARE (1024u*1024u)
typedef struct {
    WDFDMAENABLER dma;
    volatile UCHAR *core;
    struct a04_gpu_firmware_state firmware;
    struct a04_ge8300_status status;
    BOOLEAN referenced, attempted;
} GPU_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(GPU_CONTEXT,GpuContext)
EVT_WDF_DRIVER_DEVICE_ADD GpuAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE GpuPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE GpuRelease;
EVT_WDF_DEVICE_D0_ENTRY GpuD0Entry;
EVT_WDF_DEVICE_D0_EXIT GpuD0Exit;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL GpuIoControl;
static NTSTATUS read_firmware(PVOID *buffer,ULONG *bytes)
{
    UNICODE_STRING path=RTL_CONSTANT_STRING(L"\\SystemRoot\\System32\\drivers\\ge8300-mips.fw");
    OBJECT_ATTRIBUTES attr; IO_STATUS_BLOCK io; FILE_STANDARD_INFORMATION info;
    HANDLE file=NULL; NTSTATUS status; PVOID data=NULL;
    InitializeObjectAttributes(&attr,&path,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
    status=ZwCreateFile(&file,GENERIC_READ|SYNCHRONIZE,&attr,&io,NULL,FILE_ATTRIBUTE_NORMAL,
                        FILE_SHARE_READ,FILE_OPEN,FILE_SYNCHRONOUS_IO_NONALERT|FILE_NON_DIRECTORY_FILE,NULL,0);
    if (!NT_SUCCESS(status)) return status;
    status=ZwQueryInformationFile(file,&io,&info,sizeof(info),FileStandardInformation);
    if (!NT_SUCCESS(status)) goto done;
    if (info.EndOfFile.QuadPart<=0 || info.EndOfFile.QuadPart>MAX_FIRMWARE || (info.EndOfFile.QuadPart&4095)) {
        status=STATUS_INVALID_IMAGE_FORMAT; goto done;
    }
    *bytes=(ULONG)info.EndOfFile.QuadPart;
    data=ExAllocatePoolWithTag(NonPagedPoolNx,*bytes,'0XGA');
    if (!data) { status=STATUS_INSUFFICIENT_RESOURCES; goto done; }
    status=ZwReadFile(file,NULL,NULL,NULL,&io,data,*bytes,NULL,NULL);
    if (!NT_SUCCESS(status) || io.Information!=*bytes) { if (NT_SUCCESS(status)) status=STATUS_END_OF_FILE; goto done; }
    *buffer=data; data=NULL;
done:
    if (data) ExFreePoolWithTag(data,'0XGA');
    ZwClose(file); return status;
}
NTSTATUS GpuPrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
    GPU_CONTEXT *c=GpuContext(device); ULONG i, count=WdfCmResourceListGetCount(translated);
    PHYSICAL_ADDRESS core; BOOLEAN have_core=FALSE,have_mfg=FALSE;
    UNREFERENCED_PARAMETER(raw); core.QuadPart=0;
    for(i=0;i<count;++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d=WdfCmResourceListGetDescriptor(translated,i);
        if(!d || d->Type!=CmResourceTypeMemory) continue;
        if(d->u.Memory.Start.QuadPart==CORE_BASE && d->u.Memory.Length==CORE_SIZE && !have_core) {
            core=d->u.Memory.Start; have_core=TRUE;
        } else if(d->u.Memory.Start.QuadPart==MFG_BASE && d->u.Memory.Length==MFG_SIZE && !have_mfg) have_mfg=TRUE;
        else return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    if(!have_core || !have_mfg) return STATUS_DEVICE_CONFIGURATION_ERROR;
    c->core=MmMapIoSpace(core,CORE_SIZE,MmNonCached);
    return c->core ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
}
NTSTATUS GpuRelease(WDFDEVICE device,WDFCMRESLIST translated)
{
    GPU_CONTEXT *c=GpuContext(device); UNREFERENCED_PARAMETER(translated);
    /* Never detach mappings or DMA ownership while the GPU might master RAM. */
    if(c->firmware.processor_released) return STATUS_DEVICE_BUSY;
    if(c->core) { MmUnmapIoSpace((PVOID)c->core,CORE_SIZE); c->core=NULL; }
    return STATUS_SUCCESS;
}
NTSTATUS GpuD0Entry(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
    GPU_CONTEXT *c=GpuContext(device); NTSTATUS status; PVOID image=NULL; ULONG bytes=0,lo,hi;
    PDMA_ADAPTER adapter;
    UNREFERENCED_PARAMETER(previous);
    if(c->attempted) return STATUS_SUCCESS;
    c->attempted=TRUE; c->status.version=1;
#if !A04_GE8300_BOOT_TEST
    c->status.boot_status=STATUS_NOT_SUPPORTED;
    return STATUS_SUCCESS;
#else
    c->status.attempted=1;
    if(!c->core || !c->dma) { c->status.boot_status=STATUS_DEVICE_NOT_READY; return STATUS_SUCCESS; }
    status=read_firmware(&image,&bytes);
    if(!NT_SUCCESS(status)) { c->status.boot_status=status; return STATUS_SUCCESS; }
    lo=READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x20));
    hi=READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x24));
    c->status.bvnc_low=lo; c->status.bvnc_high=hi;
    adapter=WdfDmaEnablerWdmGetDmaAdapter(c->dma,WdfDmaDirectionReadFromDevice);
    if(!adapter) status=STATUS_DEVICE_CONFIGURATION_ERROR;
    else {
        WdfObjectReference(c->dma); c->referenced=TRUE;
        status=a04_gpu_firmware_boot(&c->firmware,adapter,c->core,CORE_SIZE,image,bytes,
                                   ((uint64_t)hi<<32)|lo,A04_GE8300_ALLOW_MISMATCH,26000000u);
    }
    c->status.boot_status=status;
    ExFreePoolWithTag(image,'0XGA');
    if(NT_SUCCESS(status)) {
        c->status.health_status=a04_gpu_firmware_health(&c->firmware);
        if(NT_SUCCESS(c->status.health_status)) c->status.shutdown_status=a04_gpu_firmware_shutdown(&c->firmware);
    }
    c->status.processor_released=c->firmware.processor_released;
    c->status.firmware_started=c->firmware.firmware_started;
    c->status.queue_faulted=c->firmware.queue_faulted;
    if(c->referenced && !c->firmware.processor_released) { WdfObjectDereference(c->dma); c->referenced=FALSE; }
    return STATUS_SUCCESS;
#endif
}
NTSTATUS GpuD0Exit(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
    GPU_CONTEXT *c=GpuContext(device); UNREFERENCED_PARAMETER(target);
    return c->firmware.processor_released ? STATUS_DEVICE_BUSY : STATUS_SUCCESS;
}
void GpuIoControl(WDFQUEUE queue,WDFREQUEST request,size_t output,size_t input,ULONG control)
{
    WDFDEVICE device=WdfIoQueueGetDevice(queue); GPU_CONTEXT *c=GpuContext(device);
    struct a04_ge8300_status *out; NTSTATUS status; size_t size;
    UNREFERENCED_PARAMETER(output); UNREFERENCED_PARAMETER(input);
    if(control!=A04_GE8300_IOCTL_QUERY) { WdfRequestComplete(request,STATUS_INVALID_DEVICE_REQUEST); return; }
    status=WdfRequestRetrieveOutputBuffer(request,sizeof(*out),(PVOID *)&out,&size);
    if(NT_SUCCESS(status)) { *out=c->status; WdfRequestCompleteWithInformation(request,STATUS_SUCCESS,sizeof(*out)); }
    else WdfRequestComplete(request,status);
}
NTSTATUS GpuAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp; WDF_OBJECT_ATTRIBUTES attr;
    WDF_DMA_ENABLER_CONFIG dma_config; WDF_IO_QUEUE_CONFIG queue_config;
    WDFDEVICE device; GPU_CONTEXT *c; NTSTATUS status;
    DECLARE_CONST_UNICODE_STRING(name,L"\\Device\\A04GE8300");
    DECLARE_CONST_UNICODE_STRING(link,L"\\DosDevices\\A04GE8300");
    DECLARE_CONST_UNICODE_STRING(sddl,L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
    UNREFERENCED_PARAMETER(driver);
    status=WdfDeviceInitAssignName(init,&name); if(!NT_SUCCESS(status)) return status;
    status=WdfDeviceInitAssignSDDLString(init,&sddl); if(!NT_SUCCESS(status)) return status;
    WdfDeviceInitSetDeviceType(init,FILE_DEVICE_UNKNOWN);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp); pnp.EvtDevicePrepareHardware=GpuPrepare;
    pnp.EvtDeviceReleaseHardware=GpuRelease; pnp.EvtDeviceD0Entry=GpuD0Entry; pnp.EvtDeviceD0Exit=GpuD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,GPU_CONTEXT); attr.ExecutionLevel=WdfExecutionLevelPassive;
    status=WdfDeviceCreate(&init,&attr,&device); if(!NT_SUCCESS(status)) return status;
    c=GpuContext(device); status=WdfDeviceCreateSymbolicLink(device,&link); if(!NT_SUCCESS(status)) return status;
    WDF_DMA_ENABLER_CONFIG_INIT(&dma_config,WdfDmaProfileScatterGatherDuplex,A04_PVR_HEAP_SIZE);
    status=WdfDmaEnablerCreate(device,&dma_config,WDF_NO_OBJECT_ATTRIBUTES,&c->dma); if(!NT_SUCCESS(status)) return status;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue_config,WdfIoQueueDispatchSequential);
    queue_config.EvtIoDeviceControl=GpuIoControl;
    return WdfIoQueueCreate(device,&queue_config,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING path)
{
    WDF_DRIVER_CONFIG config; WDF_DRIVER_CONFIG_INIT(&config,GpuAdd);
    return WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
