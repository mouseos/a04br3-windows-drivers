/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * GE8300 PnP firmware bring-up. Disabled by default. The controlled WinPE
 * build starts, health-checks and shuts the firmware down once. */
#include <ntddk.h>
#include <wdf.h>
#include "windows_firmware.h"
#include "windows_queue_test.h"
#include "windows_linear_context.h"
#include "ge8300_status.h"
#include "pvr_rogue_fwif.h"
#include "pvr_rogue_defs.h"
#ifndef A04_GE8300_BOOT_TEST
#define A04_GE8300_BOOT_TEST 0
#endif
#ifndef A04_GE8300_ALLOW_MISMATCH
#define A04_GE8300_ALLOW_MISMATCH 0
#endif
#ifndef A04_GE8300_COPY_SERVICE
#define A04_GE8300_COPY_SERVICE 0
#endif
#ifndef A04_GE8300_SHARED_TRACE_DIAGNOSTIC
#define A04_GE8300_SHARED_TRACE_DIAGNOSTIC 0
#endif
#if A04_GE8300_SHARED_TRACE_DIAGNOSTIC && !A04_GE8300_BOOT_TEST
#error Shared trace diagnostic requires the explicit firmware boot test
#endif
#if A04_GE8300_SHARED_TRACE_DIAGNOSTIC
#include "render_trace_shared.h"
#endif
#if A04_GE8300_COPY_SERVICE && (!A04_GE8300_BOOT_TEST || !A04_GE8300_ALLOW_MISMATCH)
#error The experimental copy service requires explicit firmware boot and BVNC mismatch opt-ins
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
    struct a04_gpu_queue_test_memory queue_memory;
#if A04_GE8300_COPY_SERVICE
    struct a04_windows_linear_context linear;
    BOOLEAN copy_online;
#endif
    struct a04_ge8300_status status;
    BOOLEAN referenced, attempted;
} GPU_CONTEXT;
#if A04_GE8300_BOOT_TEST
/* WinPE-only crash diagnostics. UART TX is shared with the existing monitor;
 * writes are bounded and never change its configuration. */
static volatile ULONG *GpuUart;
static KBUGCHECK_CALLBACK_RECORD GpuBugRecord;
static volatile ULONG_PTR *GpuBugData;
static BOOLEAN GpuBugRegistered;
static void GpuPrint(const char *s)
{
    ULONG budget;
    if (!GpuUart) return;
    while (*s) {
        for (budget=100000;budget;--budget)
            if (!(READ_REGISTER_ULONG(GpuUart+3)&0x80u) && (READ_REGISTER_ULONG(GpuUart+5)&0x20u)) break;
        if (!budget) return;
        WRITE_REGISTER_ULONG(GpuUart,(ULONG)(UCHAR)*s++);
    }
}
static void GpuHex(ULONG value)
{
    static const char digits[]="0123456789ABCDEF"; char text[9]; unsigned i;
    for(i=0;i<8;++i) text[i]=digits[(value>>(28-4*i))&15u];
    text[8]=0; GpuPrint(text);
}
static void GpuTraceStage(void *context,ULONG stage)
{ UNREFERENCED_PARAMETER(context); GpuPrint("A04GPU STEP="); GpuHex(stage); GpuPrint("\n"); }
#if A04_GE8300_SHARED_TRACE_DIAGNOSTIC
/* Output only masks and counts, never raw trace words, timestamps or pointers.
 * Firmware can write concurrently: these are candidates, not validated packets. */
static void GpuSharedTrace(GPU_CONTEXT *c)
{
    struct a04_gpu_firmware_state *s=&c->firmware;
    struct rogue_fwif_tracebuf *ctl; struct a04_shared_trace_summary result={0};
    unsigned t; ULONG before[2],after[2]; NTSTATUS status;
    if (!s->processor_released || !s->memory.heap) return;
    ctl=(void *)((uint8_t *)s->memory.heap+s->runtime.trace_control);
    status=a04_gpu_dma_sync(ctl,sizeof(*ctl),TRUE);
    if (!NT_SUCCESS(status)) goto failure;
    for(t=0;t<2;++t) before[t]=ctl->tracebuf[t].trace_pointer;
    for(t=0;t<2;++t) {
        volatile uint32_t *words=(void *)((uint8_t *)s->memory.heap+s->runtime.trace[t]);
        status=a04_gpu_dma_sync((void *)words,ROGUE_FW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS*sizeof(*words),TRUE);
        if (!NT_SUCCESS(status)) goto failure;
        a04_shared_trace_scan(words,ROGUE_FW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS,&result);
    }
    status=a04_gpu_dma_sync(ctl,sizeof(*ctl),TRUE);
    if (!NT_SUCCESS(status)) goto failure;
    for(t=0;t<2;++t) after[t]=ctl->tracebuf[t].trace_pointer;
    GpuPrint("A04GPU SHARED CANDIDATES="); GpuHex(result.candidates);
    GpuPrint(" STORE_MASK="); GpuHex(result.store_mask);
    GpuPrint(" LOAD_MASK="); GpuHex(result.load_mask);
    GpuPrint(" OTHER="); GpuHex(result.other);
    GpuPrint(" DIFFERENT="); GpuHex(result.different);
    GpuPrint(" CHANGED="); GpuHex(before[0]!=after[0] || before[1]!=after[1]); GpuPrint("\n");
    return;
failure:
    GpuPrint("A04GPU SHARED SYNC_ERROR="); GpuHex((ULONG)status); GpuPrint("\n");
}
#endif
static void GpuBugCallback(PVOID buffer,ULONG length)
{
    unsigned i; UNREFERENCED_PARAMETER(buffer); UNREFERENCED_PARAMETER(length);
    GpuPrint("\nA04GPU BUGCHECK");
    if (GpuBugData) for(i=0;i<5;++i) { GpuPrint(" "); GpuHex((ULONG)GpuBugData[i]); }
    else GpuPrint(" DATA_UNAVAILABLE");
    GpuPrint("\n");
}
#endif
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
static void GpuSnapshot(GPU_CONTEXT *c,BOOLEAN read_registers)
{
    struct a04_gpu_firmware_state *s=&c->firmware; uint8_t *heap=s->memory.heap;
    struct rogue_fwif_ccb_ctl *ctl; struct rogue_fwif_connection_ctl *conn;
    struct rogue_fwif_sysdata *sys; struct rogue_fwif_osdata *os; struct rogue_fwif_tracebuf *trace;
    uint32_t *ret,*power,*words; uint64_t count; unsigned i;
    if (!heap || !s->processor_released || !s->io.read32 || !s->io.read64) return;
    ctl=(void *)(heap+s->runtime.kernel_ccb_control); ret=(void *)(heap+s->runtime.kernel_return_slots);
    power=(void *)(heap+s->runtime.power_sync); sys=(void *)(heap+s->runtime.system_data);
    os=(void *)(heap+s->runtime.os_data); trace=(void *)(heap+s->runtime.trace_control);
    words=(void *)(heap+s->runtime.trace[0]); conn=(void *)(heap+A04_PVR_HEAP_SIZE-3u*65536u);
#define SYNC(p,n,bit) do { if (!NT_SUCCESS(a04_gpu_dma_sync((p),(ULONG)(n),TRUE))) c->status.sync_error|=(bit); } while (0)
    SYNC(ctl,sizeof(*ctl),1u); SYNC(ret,sizeof(*ret),2u); SYNC(power,sizeof(*power),4u);
    SYNC(sys,sizeof(*sys),8u); SYNC(os,sizeof(*os),16u); SYNC(trace,sizeof(*trace),32u);
    SYNC(words,4u*sizeof(*words),64u); SYNC(conn,sizeof(*conn),128u);
#undef SYNC
    c->status.queue_read=ctl->read_offset; c->status.queue_write=ctl->write_offset;
    c->status.queue_return=ret[0]; c->status.power_sync=*power;
    c->status.connection_fw=conn->connection_fw_state; c->status.connection_os=conn->connection_os_state;
    c->status.fw_power_state=sys->pow_state; c->status.commands_executed=os->kccb_cmds_executed;
    c->status.trace_pointer=trace->tracebuf[0].trace_pointer;
    for (i=0;i<4;++i) c->status.trace_words[i]=words[i];
    if (!read_registers) return;
    c->status.mts_enable=s->io.read32(s->io.context,ROGUE_CR_MTS_SCHEDULE_ENABLE);
    c->status.mts_bg=s->io.read32(s->io.context,ROGUE_CR_MTS_BGCTX);
    c->status.mts_int=s->io.read32(s->io.context,ROGUE_CR_MTS_INTCTX);
    count=s->io.read64(s->io.context,ROGUE_CR_MTS_BGCTX_COUNTED_SCHEDULE);
    c->status.mts_count_low=(ULONG)count; c->status.mts_count_high=(ULONG)(count>>32);
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
    if(!c->core) return STATUS_INSUFFICIENT_RESOURCES;
#if A04_GE8300_BOOT_TEST
    {
        PHYSICAL_ADDRESS uart; UNICODE_STRING name=RTL_CONSTANT_STRING(L"KiBugCheckData");
        uart.QuadPart=0x11005000u;
        GpuUart=MmMapIoSpace(uart,0x1000u,MmNonCached);
        if (!GpuUart) { MmUnmapIoSpace((PVOID)c->core,CORE_SIZE); c->core=NULL; return STATUS_INSUFFICIENT_RESOURCES; }
        GpuBugData=(volatile ULONG_PTR *)MmGetSystemRoutineAddress(&name);
        KeInitializeCallbackRecord(&GpuBugRecord);
        GpuBugRegistered=KeRegisterBugCheckCallback(&GpuBugRecord,GpuBugCallback,NULL,0,(PUCHAR)"A04GPU");
        if (!GpuBugRegistered) {
            MmUnmapIoSpace((PVOID)GpuUart,0x1000u); GpuUart=NULL;
            MmUnmapIoSpace((PVOID)c->core,CORE_SIZE); c->core=NULL; return STATUS_UNSUCCESSFUL;
        }
        c->firmware.trace=GpuTraceStage; c->firmware.trace_context=c;
        GpuPrint(GpuBugData ? "A04GPU BUGCHECK CAPTURE READY\n" : "A04GPU BUGCHECK DATA UNAVAILABLE\n");
    }
#endif
    return STATUS_SUCCESS;
}
NTSTATUS GpuRelease(WDFDEVICE device,WDFCMRESLIST translated)
{
    GPU_CONTEXT *c=GpuContext(device); UNREFERENCED_PARAMETER(translated);
    /* Never detach mappings or DMA ownership while the GPU might master RAM. */
    if(c->firmware.processor_released) return STATUS_DEVICE_BUSY;
#if A04_GE8300_COPY_SERVICE
    if(c->linear.memory.cpu || c->linear.memory.adapter || c->referenced) return STATUS_DEVICE_BUSY;
#endif
#if A04_GE8300_BOOT_TEST
    if(GpuBugRegistered) { KeDeregisterBugCheckCallback(&GpuBugRecord); GpuBugRegistered=FALSE; }
    if(GpuUart) { MmUnmapIoSpace((PVOID)GpuUart,0x1000u); GpuUart=NULL; }
#endif
    if(c->core) { MmUnmapIoSpace((PVOID)c->core,CORE_SIZE); c->core=NULL; }
    return STATUS_SUCCESS;
}
NTSTATUS GpuD0Entry(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
    GPU_CONTEXT *c=GpuContext(device); NTSTATUS status; PVOID image=NULL; ULONG bytes=0,lo,hi;
    PDMA_ADAPTER adapter;
    UNREFERENCED_PARAMETER(previous);
    if(c->attempted) return STATUS_SUCCESS;
    c->attempted=TRUE; c->status.version=5;
#if !A04_GE8300_BOOT_TEST
    c->status.boot_status=STATUS_NOT_SUPPORTED;
    return STATUS_SUCCESS;
#else
    c->status.attempted=1;
    c->status.mmu_status=STATUS_NOT_SUPPORTED;
    c->status.queue_status=STATUS_NOT_SUPPORTED;
    c->status.shutdown_status=STATUS_NOT_SUPPORTED;
    GpuTraceStage(c,0x100);
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
    GpuTraceStage(c,0x101);
    ExFreePoolWithTag(image,'0XGA');
    if(NT_SUCCESS(status)) {
        c->status.health_status=a04_gpu_firmware_health(&c->firmware);
        GpuTraceStage(c,0x102);
        GpuSnapshot(c,TRUE);
        if(NT_SUCCESS(c->status.health_status)) {
#if A04_GE8300_COPY_SERVICE
            c->status.queue_status=a04_windows_linear_context_start(&c->linear,&c->firmware);
            c->status.mmu_status=c->status.queue_status;
            c->copy_online=NT_SUCCESS(c->status.queue_status);
            GpuPrint("A04GPU SERVICE STATUS="); GpuHex(c->status.queue_status); GpuPrint("\n");
            /* Avoid serial traffic on every display chunk; bugcheck capture
             * stays registered and shutdown diagnostics restore tracing. */
            if (c->copy_online) c->firmware.trace=NULL;
#else
            c->status.mmu_status=a04_gpu_firmware_invalidate_mmu(&c->firmware);
            GpuTraceStage(c,0x103);
            if(NT_SUCCESS(c->status.mmu_status)) {
                c->status.queue_status=a04_gpu_queue_test(&c->firmware,&c->queue_memory);
                GpuTraceStage(c,0x104);
            }
#if A04_GE8300_SHARED_TRACE_DIAGNOSTIC
            GpuSharedTrace(c);
#endif
            if(NT_SUCCESS(c->status.mmu_status) && NT_SUCCESS(c->status.queue_status))
                c->status.shutdown_status=a04_gpu_firmware_shutdown(&c->firmware);
            if(NT_SUCCESS(c->status.shutdown_status))
                c->status.shutdown_status=a04_gpu_queue_test_free(&c->firmware,&c->queue_memory);
            /* Power-off may have gated MMIO; only inspect retained memory. */
            if(!NT_SUCCESS(c->status.shutdown_status) && c->firmware.processor_released)
                GpuSnapshot(c,FALSE);
#endif
        }
    } else if(c->firmware.processor_released && c->firmware.runtime.system_init) GpuSnapshot(c,TRUE);
    c->status.processor_released=c->firmware.processor_released;
    c->status.firmware_started=c->firmware.firmware_started;
    c->status.queue_faulted=c->firmware.queue_faulted;
    if(c->referenced && !c->firmware.processor_released && !c->queue_memory.cpu) {
        WdfObjectDereference(c->dma); c->referenced=FALSE;
    }
    return STATUS_SUCCESS;
#endif
}
NTSTATUS GpuD0Exit(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
    GPU_CONTEXT *c=GpuContext(device); UNREFERENCED_PARAMETER(target);
#if A04_GE8300_COPY_SERVICE
    NTSTATUS status;
    GpuPrint("A04GPU D0EXIT TARGET="); GpuHex((ULONG)target);
    GpuPrint(" ONLINE="); GpuHex((ULONG)c->copy_online); GpuPrint("\n");
    c->copy_online=FALSE;
    c->firmware.trace=GpuTraceStage;
    if (c->firmware.processor_released) {
        status=a04_gpu_firmware_shutdown(&c->firmware);
        if (!NT_SUCCESS(status)) return status;
    }
    status=a04_windows_linear_context_free(&c->linear,&c->firmware);
    if (!NT_SUCCESS(status)) return status;
    if (c->referenced) { WdfObjectDereference(c->dma); c->referenced=FALSE; }
    /* Do not restart firmware on resume until the SoC power, clocks and
     * display ownership have a validated reinitialization sequence. The
     * display client falls back to CPU while this device remains present. */
#endif
    return c->firmware.processor_released ? STATUS_DEVICE_BUSY : STATUS_SUCCESS;
}
void GpuIoControl(WDFQUEUE queue,WDFREQUEST request,size_t output,size_t input,ULONG control)
{
    WDFDEVICE device=WdfIoQueueGetDevice(queue); GPU_CONTEXT *c=GpuContext(device);
    struct a04_ge8300_status *out; NTSTATUS status; size_t size;
#if A04_GE8300_COPY_SERVICE
    if (control==A04_GE8300_IOCTL_REPORT_DISPLAY) {
        ULONG *value; size_t bytes;
        status=WdfRequestRetrieveInputBuffer(request,sizeof(*value),(PVOID*)&value,&bytes);
        if (!NT_SUCCESS(status) || input!=sizeof(*value) || *value>1u) {
            WdfRequestComplete(request,STATUS_INVALID_PARAMETER); return;
        }
        GpuPrint(*value ? "A04GPU DISPLAY PNP STARTED\n" : "A04GPU DISPLAY PNP FAILED\n");
        WdfRequestComplete(request,STATUS_SUCCESS); return;
    }
    if (control==A04_GE8300_IOCTL_COPY) {
        void *src,*dst; size_t in_size,out_size;
        if (WdfRequestGetRequestorMode(request)!=KernelMode) {
            WdfRequestComplete(request,STATUS_ACCESS_DENIED); return;
        }
        if (!c->copy_online || input!=A04_GE8300_COPY_BYTES || output!=A04_GE8300_COPY_BYTES) {
            WdfRequestComplete(request,STATUS_INVALID_PARAMETER); return;
        }
        status=WdfRequestRetrieveInputBuffer(request,A04_GE8300_COPY_BYTES,&src,&in_size);
        if (NT_SUCCESS(status)) status=WdfRequestRetrieveOutputBuffer(request,A04_GE8300_COPY_BYTES,&dst,&out_size);
        if (NT_SUCCESS(status)) status=a04_windows_linear_context_copy(&c->linear,src,dst);
        if (NT_SUCCESS(status)) {
            if (++c->status.copy_completed==1u) GpuPrint("A04GPU DISPLAY COPY COMPLETE\n");
        } else {
            ++c->status.copy_failed; c->copy_online=FALSE;
            GpuPrint("A04GPU DISPLAY COPY FAILED STATUS="); GpuHex((ULONG)status); GpuPrint("\n");
        }
        WdfRequestCompleteWithInformation(request,status,NT_SUCCESS(status) ? A04_GE8300_COPY_BYTES : 0);
        return;
    }
#endif
    UNREFERENCED_PARAMETER(output); UNREFERENCED_PARAMETER(input);
    if(control!=A04_GE8300_IOCTL_QUERY) { WdfRequestComplete(request,STATUS_INVALID_DEVICE_REQUEST); return; }
#if A04_GE8300_COPY_SERVICE
    c->status.processor_released=c->firmware.processor_released;
    c->status.firmware_started=c->firmware.firmware_started;
    c->status.queue_faulted=c->firmware.queue_faulted;
#if A04_GE8300_SHARED_TRACE_DIAGNOSTIC
    GpuSharedTrace(c);
#endif
#endif
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
    /* The framework drains delivered requests before D0 exit. Each transfer
     * is synchronous and bounded; no request is held for later completion. */
    queue_config.PowerManaged=WdfTrue;
    queue_config.EvtIoDeviceControl=GpuIoControl;
    return WdfIoQueueCreate(device,&queue_config,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING path)
{
    WDF_DRIVER_CONFIG config; WDF_DRIVER_CONFIG_INIT(&config,GpuAdd);
    return WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
