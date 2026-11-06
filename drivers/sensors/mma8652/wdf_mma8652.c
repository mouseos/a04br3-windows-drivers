/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental sensor HID lower filter; D0 remains gated until firmware
 * provides a trustworthy per-boot handoff and ownership is coordinated.
 */
#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>
#include <acpiioct.h>
#include "mma8652.h"
#include "mt8167_i2c.h"
#include "sensor_hid.h"
#define I2C1_BASE 0x1100a000u
#define I2C1_SIZE 0x1000u
#ifndef MMA8652_UART_TRACE
#define MMA8652_UART_TRACE 0
#endif
typedef struct {
    volatile UCHAR *i2c; volatile ULONG *handoff; WDFWAITLOCK lock; WDFQUEUE reads; WDFTIMER timer;
    struct a04_i2c_io io; struct a04_mma_i2c adapter; struct a04_mma8652_bus bus;
    struct a04_mma_hid_features features; struct a04_mma8652_sample last;
    volatile LONG online; BOOLEAN faulted, have_last;
    volatile ULONG *uart; ULONG trace_count;
    LARGE_INTEGER counter_frequency; ULONG samples;
} ACC_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(ACC_CONTEXT, AccContext)
static void trace(ACC_CONTEXT *c, const char *event, ULONG value)
{
#if MMA8652_UART_TRACE
    char line[96]; unsigned n=0, i, spin;
    const char *prefix="MMA8652 ", *digits="0123456789ABCDEF";
    if (!c->uart) return;
    while (*prefix) line[n++]=*prefix++;
    while (*event && n<80) line[n++]=*event++;
    line[n++]='=';
    for(i=0;i<8;i++) line[n++]=digits[(value>>(28-4*i))&15u];
    line[n++]='\n';
    for(i=0;i<n;i++) {
        for(spin=100000;spin;--spin)
            if (!(READ_REGISTER_ULONG(c->uart+3)&0x80u) && (READ_REGISTER_ULONG(c->uart+5)&0x20u)) break;
        if(!spin) return;
        WRITE_REGISTER_ULONG(c->uart,(ULONG)(UCHAR)line[i]);
    }
#else
    UNREFERENCED_PARAMETER(c); UNREFERENCED_PARAMETER(event); UNREFERENCED_PARAMETER(value);
#endif
}
static NTSTATUS fail(ACC_CONTEXT *c, const char *event, NTSTATUS status)
{ trace(c,event,(ULONG)status); return status; }
static VOID AccCleanup(WDFOBJECT object)
{
    ACC_CONTEXT *c=AccContext((WDFDEVICE)object);
    if(c->uart) { MmUnmapIoSpace((PVOID)c->uart,0x1000); c->uart=NULL; }
}
EVT_WDF_DRIVER_DEVICE_ADD AccAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE AccPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE AccRelease;
EVT_WDF_DEVICE_D0_ENTRY AccD0;
EVT_WDF_DEVICE_D0_EXIT AccD0Exit;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL AccIoctl;
EVT_WDF_TIMER AccTimer;
static uint16_t rd(void *p,unsigned off) { ACC_CONTEXT *c=p; return READ_REGISTER_USHORT((volatile USHORT *)(c->i2c+off)); }
static void wr(void *p,unsigned off,uint16_t v) { ACC_CONTEXT *c=p; WRITE_REGISTER_USHORT((volatile USHORT *)(c->i2c+off),v); }
static void delay(void *p,unsigned us) { UNREFERENCED_PARAMETER(p); KeStallExecutionProcessor(us); }
/* KeQueryInterruptTime is tick-quantized: a tick can exceed the I2C 10 ms
 * deadline even when the transaction has just completed. Use the hardware
 * performance counter and split the conversion to avoid uptime overflow. */
static uint64_t now_ns(void *p)
{
    ACC_CONTEXT *c=p;
    uint64_t ticks=(uint64_t)KeQueryPerformanceCounter(NULL).QuadPart;
    uint64_t frequency=(uint64_t)c->counter_frequency.QuadPart;
    return (ticks/frequency)*UINT64_C(1000000000)+
        ((ticks%frequency)*UINT64_C(1000000000))/frequency;
}
/* HSTA checks the per-boot ACPI NVS marker and shared clock/pin state.
 * This is an ownership contract, not authentication against hostile firmware. */
static BOOLEAN handoff_qualified(WDFDEVICE dev, ACC_CONTEXT *c)
{
 ACPI_EVAL_INPUT_BUFFER input; ACPI_EVAL_OUTPUT_BUFFER output;
 WDF_MEMORY_DESCRIPTOR in, out; NTSTATUS status; ULONG_PTR length=0;
 if (!c->handoff || READ_REGISTER_ULONG(c->handoff)!=0x41303431u) { trace(c,"HANDOFF_MARKER",c->handoff?READ_REGISTER_ULONG(c->handoff):0); return FALSE; }
 RtlZeroMemory(&input,sizeof(input)); RtlZeroMemory(&output,sizeof(output));
 input.Signature=ACPI_EVAL_INPUT_BUFFER_SIGNATURE; input.MethodNameAsUlong='ATSH';
 WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&in,&input,sizeof(input));
 WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&out,&output,sizeof(output));
 status=WdfIoTargetSendIoctlSynchronously(WdfDeviceGetIoTarget(dev),NULL,
   IOCTL_ACPI_EVAL_METHOD,&in,&out,NULL,&length);
 trace(c,"HSTA_STATUS",(ULONG)status); trace(c,"HSTA_LENGTH",(ULONG)length);
 trace(c,"HSTA_VALUE",output.Argument[0].Argument);
 if (!NT_SUCCESS(status) || length<sizeof(output) ||
     output.Signature!=ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE || output.Count!=1 ||
     output.Argument[0].Type!=ACPI_METHOD_ARGUMENT_INTEGER || output.Argument[0].Argument!=1) return FALSE;
 return READ_REGISTER_ULONG(c->handoff)==0x41303431u;
}
NTSTATUS AccPrepare(WDFDEVICE dev, WDFCMRESLIST raw, WDFCMRESLIST translated)
{
    ACC_CONTEXT *c=AccContext(dev); PHYSICAL_ADDRESS address, handoff; ULONG i, found=0, seen=0;
    UNREFERENCED_PARAMETER(raw);
    for (i=0;i<WdfCmResourceListGetCount(translated);++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d=WdfCmResourceListGetDescriptor(translated,i);
        if (!d || d->Type!=CmResourceTypeMemory) continue;
        trace(c,"RESOURCE_BASE",d->u.Memory.Start.LowPart); trace(c,"RESOURCE_SIZE",d->u.Memory.Length);
        if (d->u.Memory.Length!=I2C1_SIZE) return fail(c,"RESOURCE_SIZE_FAIL",STATUS_DEVICE_CONFIGURATION_ERROR);
        if (d->u.Memory.Start.QuadPart==I2C1_BASE && !found) { address=d->u.Memory.Start; found=1; }
        else if (!seen && d->u.Memory.Start.QuadPart>0 && d->u.Memory.Start.QuadPart!=I2C1_BASE && d->u.Memory.Start.QuadPart<=0xfffff000u &&
                 (d->u.Memory.Start.LowPart & 0xfffu)==0) { handoff=d->u.Memory.Start; seen=1; }
        else return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    if (!found || !seen) return fail(c,"RESOURCE_MISSING",STATUS_DEVICE_CONFIGURATION_ERROR);
    c->handoff=MmMapIoSpace(handoff,I2C1_SIZE,MmCached);
    if (!c->handoff) return STATUS_INSUFFICIENT_RESOURCES;
    c->i2c=MmMapIoSpace(address,I2C1_SIZE,MmNonCached);
    if (!c->i2c) { MmUnmapIoSpace((PVOID)c->handoff,I2C1_SIZE); c->handoff=NULL; return STATUS_INSUFFICIENT_RESOURCES; }
    return STATUS_SUCCESS;
}
NTSTATUS AccRelease(WDFDEVICE dev, WDFCMRESLIST translated)
{
    ACC_CONTEXT *c=AccContext(dev); UNREFERENCED_PARAMETER(translated);
    if (c->i2c) { MmUnmapIoSpace((PVOID)c->i2c,I2C1_SIZE); c->i2c=NULL; }
    if (c->handoff) { MmUnmapIoSpace((PVOID)c->handoff,I2C1_SIZE); c->handoff=NULL; }
    return STATUS_SUCCESS;
}
NTSTATUS AccD0(WDFDEVICE dev, WDF_POWER_DEVICE_STATE previous)
{
    ACC_CONTEXT *c=AccContext(dev); int r; ULONG timing, inverse;
    UNREFERENCED_PARAMETER(previous);
    trace(c,"D0_ENTRY",previous);
    (VOID)KeQueryPerformanceCounter(&c->counter_frequency);
    if(c->counter_frequency.QuadPart<=0 || c->counter_frequency.QuadPart>1000000000)
        return fail(c,"COUNTER_FREQUENCY_FAIL",STATUS_DEVICE_CONFIGURATION_ERROR);
    if (!c->i2c || c->faulted || !handoff_qualified(dev,c)) return fail(c,"D0_HANDOFF_FAIL",STATUS_DEVICE_NOT_READY);
    timing=READ_REGISTER_ULONG(c->handoff+1);
    inverse=READ_REGISTER_ULONG(c->handoff+2);
    if (inverse!=~timing || (timing & ~0x073fu)) return fail(c,"D0_TIMING_FAIL",STATUS_DEVICE_CONFIGURATION_ERROR);
    /* I2C1 is a single exclusive resource; only after HSTA confirms the gate,
     * restore the controller configuration on each D0 entry. */
    if (rd(c,0x24)&1u) return fail(c,"D0_BUSY",STATUS_DEVICE_NOT_READY);
    wr(c,0x08,0); wr(c,0x0c,7); wr(c,0x38,1); wr(c,0x40,3);
    wr(c,0x54,0); wr(c,0x70,1); wr(c,0x20,(uint16_t)timing);
    wr(c,0x48,0); wr(c,0x28,0x8001); wr(c,0x60,0); wr(c,0x10,0x28); wr(c,0x1c,2);
    if (rd(c,0x40)!=3u || rd(c,0x70)!=1u || rd(c,0x20)!=(uint16_t)timing ||
        rd(c,0x28)!=0x8001u || (rd(c,0x48)&1u) || rd(c,0x60)!=0u)
        return fail(c,"D0_REGISTER_FAIL",STATUS_DEVICE_HARDWARE_ERROR);
    c->io.context=c; c->io.read16=rd; c->io.write16=wr; c->io.delay_us=delay;
    c->adapter.io=&c->io; c->adapter.now_ns=now_ns; c->adapter.faulted=0;
    c->bus.context=&c->adapter; c->bus.read_regs=a04_mma_i2c_read; c->bus.write_reg=a04_mma_i2c_write;
    WdfWaitLockAcquire(c->lock,NULL); r=a04_mma8652_start(&c->bus); WdfWaitLockRelease(c->lock);
    if (r) { trace(c,"SENSOR_START_RESULT",(ULONG)r); c->faulted=TRUE; return fail(c,"D0_SENSOR_FAIL",STATUS_DEVICE_HARDWARE_ERROR); }
    c->have_last=FALSE; WdfIoQueueStart(c->reads); InterlockedExchange(&c->online,1); WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(20));
    trace(c,"D0_READY",0); return STATUS_SUCCESS;
}
NTSTATUS AccD0Exit(WDFDEVICE dev,WDF_POWER_DEVICE_STATE target)
{
    ACC_CONTEXT *c=AccContext(dev); UNREFERENCED_PARAMETER(target);
    trace(c,"D0_EXIT",target);
    InterlockedExchange(&c->online,0); WdfTimerStop(c->timer,TRUE); WdfTimerStop(c->timer,FALSE);
    WdfIoQueuePurgeSynchronously(c->reads);
    /* D0 re-entry repeats ACPI gate checks and reinitializes the sensor. */
    return STATUS_SUCCESS;
}
static NTSTATUS Copy(WDFREQUEST request,const void *p,size_t len)
{
 WDFMEMORY m; size_t n; NTSTATUS s=WdfRequestRetrieveOutputMemory(request,&m);
 if (!NT_SUCCESS(s)) return s; WdfMemoryGetBuffer(m,&n); if (n<len) return STATUS_BUFFER_TOO_SMALL;
 s=WdfMemoryCopyFromBuffer(m,0,(PVOID)p,len); if (NT_SUCCESS(s)) WdfRequestSetInformation(request,len); return s;
}
VOID AccTimer(WDFTIMER timer)
{
 WDFDEVICE dev=(WDFDEVICE)WdfTimerGetParentObject(timer); ACC_CONTEXT *c=AccContext(dev);
 WDFREQUEST request; struct a04_mma8652_sample sample; uint8_t report[A04_MMA_HID_INPUT_SIZE];
 ULONG interval; int r;
 if (!InterlockedCompareExchange(&c->online,1,1)) return;
 WdfWaitLockAcquire(c->lock,NULL); interval=c->features.interval_ms;
 if (c->features.reporting && NT_SUCCESS(WdfIoQueueRetrieveNextRequest(c->reads,&request))) {
   r=a04_mma8652_read(&c->bus,&sample);
   if(r==A04_MMA_OK) {
     ++c->samples;
     if(c->samples==1 || c->samples==100 || c->samples==1000)trace(c,"SAMPLES",c->samples);
   }
   if (r==A04_MMA_OK && a04_mma_hid_changed(&c->features,&sample,&c->last,c->have_last)) { c->last=sample; c->have_last=TRUE; a04_mma_hid_input(&sample,report); WdfRequestComplete(request,Copy(request,report,sizeof(report))); }
   else if (r==A04_MMA_OK) { NTSTATUS s=WdfRequestForwardToIoQueue(request,c->reads); if (!NT_SUCCESS(s)) WdfRequestComplete(request,s); }
   else if (r==A04_MMA_NOT_READY) { NTSTATUS s=WdfRequestForwardToIoQueue(request,c->reads); if (!NT_SUCCESS(s)) WdfRequestComplete(request,s); }
   else { trace(c,"READ_FAIL",(ULONG)r); trace(c,"I2C_STATUS",rd(c,0x0c)); trace(c,"I2C_START",rd(c,0x24)); c->faulted=TRUE; InterlockedExchange(&c->online,0); WdfRequestComplete(request,STATUS_DEVICE_HARDWARE_ERROR); }
 }
 WdfWaitLockRelease(c->lock);
 if (InterlockedCompareExchange(&c->online,1,1)) WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(interval));
 else WdfIoQueuePurge(c->reads,WDF_NO_EVENT_CALLBACK,WDF_NO_CONTEXT);
}
VOID AccIoctl(WDFQUEUE queue,WDFREQUEST request,size_t outlen,size_t inlen,ULONG code)
{
 ACC_CONTEXT *c=AccContext(WdfIoQueueGetDevice(queue)); NTSTATUS s=STATUS_NOT_SUPPORTED;
 HID_DESCRIPTOR h={0}; const uint8_t *descriptor; size_t size; PIRP irp; PHID_XFER_PACKET packet;
 uint8_t feature[A04_MMA_HID_FEATURE_SIZE];
 switch (code) {
 case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
  descriptor=a04_mma_hid_descriptor(&size); UNREFERENCED_PARAMETER(descriptor);
  h.bLength=sizeof(h); h.bDescriptorType=HID_HID_DESCRIPTOR_TYPE; h.bcdHID=0x0111;
  h.bNumDescriptors=1; h.DescriptorList[0].bReportType=HID_REPORT_DESCRIPTOR_TYPE;
  h.DescriptorList[0].wReportLength=(USHORT)size; s=Copy(request,&h,sizeof(h)); break;
 case IOCTL_HID_GET_DEVICE_ATTRIBUTES: {
  /* This device is ACPI-enumerated and has no assigned USB VID/PID.
   * Zero denotes absent transport IDs; chip identity is checked in WHO_AM_I. */
  HID_DEVICE_ATTRIBUTES attributes={0}; attributes.Size=sizeof(attributes);
  attributes.VersionNumber=1; s=Copy(request,&attributes,sizeof(attributes)); break;
 }
 case IOCTL_HID_GET_REPORT_DESCRIPTOR:
  descriptor=a04_mma_hid_descriptor(&size); s=Copy(request,descriptor,size); break;
 case IOCTL_HID_READ_REPORT:
  s=WdfRequestForwardToIoQueue(request,c->reads); if (NT_SUCCESS(s)) return; break;
 case IOCTL_HID_GET_FEATURE:
 case IOCTL_HID_SET_FEATURE:
  irp=WdfRequestWdmGetIrp(request); packet=(PHID_XFER_PACKET)irp->UserBuffer;
  if ((code==IOCTL_HID_GET_FEATURE ? outlen : inlen)<sizeof(*packet) || !packet || packet->reportId!=1 || !packet->reportBuffer || packet->reportBufferLen<sizeof(feature)) { s=STATUS_INVALID_PARAMETER; break; }
  if(code==IOCTL_HID_SET_FEATURE && c->trace_count<16) {
    const UCHAR *p=packet->reportBuffer;
    trace(c,"FEATURE_WRITE_HEAD",((ULONG)p[0]<<16)|((ULONG)p[1]<<8)|p[2]);
    trace(c,"FEATURE_WRITE_INTERVAL",(ULONG)p[5]|((ULONG)p[6]<<8)|((ULONG)p[7]<<16)|((ULONG)p[8]<<24));
  }
  WdfWaitLockAcquire(c->lock,NULL);
  if (code==IOCTL_HID_GET_FEATURE) { a04_mma_hid_get_feature(&c->features,feature); RtlCopyMemory(packet->reportBuffer,feature,sizeof(feature)); s=STATUS_SUCCESS; }
  else { s=a04_mma_hid_set_feature(&c->features,packet->reportBuffer,sizeof(feature)) ? STATUS_INVALID_PARAMETER : STATUS_SUCCESS; if (NT_SUCCESS(s)) c->have_last=FALSE; }
  if(c->trace_count<16) { trace(c,"FEATURE_INTERVAL",c->features.interval_ms); trace(c,"FEATURE_REPORTING",c->features.reporting); }
  WdfWaitLockRelease(c->lock); if (NT_SUCCESS(s)) WdfRequestSetInformation(request,sizeof(feature)); break;
 }
 if(!NT_SUCCESS(s) && c->trace_count++<16) { trace(c,"IOCTL_FAIL_CODE",code); trace(c,"IOCTL_FAIL_STATUS",(ULONG)s); }
 WdfRequestComplete(request,s);
}
NTSTATUS AccAdd(WDFDRIVER driver, PWDFDEVICE_INIT init)
{
 WDF_PNPPOWER_EVENT_CALLBACKS pnp; WDF_OBJECT_ATTRIBUTES attr, child;
 WDF_IO_QUEUE_CONFIG q; WDF_TIMER_CONFIG timer; WDFDEVICE device; NTSTATUS status; ACC_CONTEXT *c;
 UNREFERENCED_PARAMETER(driver); WdfFdoInitSetFilter(init);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp); pnp.EvtDevicePrepareHardware=AccPrepare;
 pnp.EvtDeviceReleaseHardware=AccRelease; pnp.EvtDeviceD0Entry=AccD0; pnp.EvtDeviceD0Exit=AccD0Exit;
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp); WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,ACC_CONTEXT);
 attr.ExecutionLevel=WdfExecutionLevelPassive; attr.EvtCleanupCallback=AccCleanup;
 status=WdfDeviceCreate(&init,&attr,&device); if (!NT_SUCCESS(status)) return status;
 c=AccContext(device); c->features.interval_ms=20; c->features.reporting=1;
#if MMA8652_UART_TRACE
 { PHYSICAL_ADDRESS uart; uart.QuadPart=0x11005000u; c->uart=MmMapIoSpace(uart,0x1000,MmNonCached); }
#endif
 trace(c,"DEVICE_ADD",0);
 WDF_OBJECT_ATTRIBUTES_INIT(&child); child.ParentObject=device;
 status=WdfWaitLockCreate(&child,&c->lock); if (!NT_SUCCESS(status)) return status;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchParallel); q.EvtIoInternalDeviceControl=AccIoctl;
 status=WdfIoQueueCreate(device,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE); if (!NT_SUCCESS(status)) return status;
 WDF_IO_QUEUE_CONFIG_INIT(&q,WdfIoQueueDispatchManual); status=WdfIoQueueCreate(device,&q,WDF_NO_OBJECT_ATTRIBUTES,&c->reads); if (!NT_SUCCESS(status)) return status;
 WDF_TIMER_CONFIG_INIT(&timer,AccTimer); timer.AutomaticSerialization=FALSE;
 WDF_OBJECT_ATTRIBUTES_INIT(&child); child.ParentObject=device; child.ExecutionLevel=WdfExecutionLevelPassive;
 return WdfTimerCreate(&timer,&child,&c->timer);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{ WDF_DRIVER_CONFIG config; WDF_DRIVER_CONFIG_INIT(&config,AccAdd); return WdfDriverCreate(driver,registry,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE); }
