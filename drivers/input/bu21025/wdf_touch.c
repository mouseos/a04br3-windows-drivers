/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental A04BR3 HID lower filter. It requires the opt-in MOU0004 ACPI
 * device and exclusive ownership of the declared MMIO ranges. Polling only;
 * does not support wake or recovery from a controller fault.
 */
#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>
#include "board_init.h"
#include "device.h"
#include "mt8167_adapter.h"

enum { TOP, INFRA, GPIO, I2C, RANGE_COUNT };
static const ULONG addresses[RANGE_COUNT] = {0x10000000,0x10001000,0x10005000,0x11009000};
#define RANGE_SIZE 0x1000u

typedef struct {
    WDFDEVICE device;
    WDFQUEUE reads;
    WDFTIMER timer;
    volatile UCHAR *mmio[RANGE_COUNT];
    struct a04_i2c_io bus;
    struct a04_bu21025_mt8167 adapter;
    struct a04_bu21025_io io;
    struct a04_touch_device touch;
    UCHAR descriptor[A04_TOUCH_DESCRIPTOR_CAPACITY];
    size_t descriptor_length;
    volatile LONG enabled;
    BOOLEAN faulted;
} TOUCH_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(TOUCH_CONTEXT, TouchGetContext)
EVT_WDF_DRIVER_DEVICE_ADD TouchAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE TouchPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE TouchRelease;
EVT_WDF_DEVICE_D0_ENTRY TouchD0Entry;
EVT_WDF_DEVICE_D0_EXIT TouchD0Exit;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL TouchIoctl;
EVT_WDF_TIMER TouchTimer;

static uint16_t I2cRead(void *p, unsigned off)
{ return READ_REGISTER_USHORT((volatile USHORT *)(TouchGetContext((WDFDEVICE)p)->mmio[I2C]+off)); }
static void I2cWrite(void *p, unsigned off, uint16_t v)
{ WRITE_REGISTER_USHORT((volatile USHORT *)(TouchGetContext((WDFDEVICE)p)->mmio[I2C]+off), v); }
static void Delay(void *p, unsigned us)
{ UNREFERENCED_PARAMETER(p); KeStallExecutionProcessor(us); }
static int BoardUpdate(void *p, enum a04_touch_register_bank bank, unsigned off, uint32_t mask, uint32_t bits)
{
    TOUCH_CONTEXT *c = TouchGetContext((WDFDEVICE)p);
    unsigned i = bank == A04_TOUCH_GPIO ? GPIO : bank == A04_TOUCH_INFRA ? INFRA : TOP;
    volatile ULONG *reg;
    ULONG value;
    if (!c->mmio[i] || off > RANGE_SIZE-4 || (bits & ~mask)) return -1;
    reg = (volatile ULONG *)(c->mmio[i]+off);
    value = READ_REGISTER_ULONG(reg);
    WRITE_REGISTER_ULONG(reg, (value & ~mask) | bits);
    return (READ_REGISTER_ULONG(reg) & mask) == bits ? 0 : -1;
}
static int BoardWrite(void *p, enum a04_touch_register_bank bank, unsigned off, uint32_t value)
{
    TOUCH_CONTEXT *c = TouchGetContext((WDFDEVICE)p);
    unsigned i = bank == A04_TOUCH_GPIO ? GPIO : TOP;
    if (!c->mmio[i] || off > RANGE_SIZE-4) return -1;
    WRITE_REGISTER_ULONG((volatile ULONG *)(c->mmio[i]+off), value);
    return 0;
}
static int BoardI2cWrite(void *p, unsigned off, uint16_t value)
{ I2cWrite(p,off,value); return 0; }

NTSTATUS TouchPrepare(WDFDEVICE device, WDFCMRESLIST raw, WDFCMRESLIST translated)
{
    TOUCH_CONTEXT *c=TouchGetContext(device);
    PHYSICAL_ADDRESS found[RANGE_COUNT];
    BOOLEAN seen[RANGE_COUNT]={FALSE,FALSE,FALSE,FALSE};
    ULONG i,j;
    UNREFERENCED_PARAMETER(raw);
    for (i=0;i<WdfCmResourceListGetCount(translated);++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d=WdfCmResourceListGetDescriptor(translated,i);
        if (!d || d->Type != CmResourceTypeMemory) continue;
        for (j=0;j<RANGE_COUNT;++j)
            if (d->u.Memory.Start.QuadPart==addresses[j] && d->u.Memory.Length==RANGE_SIZE && !seen[j]) break;
        if (j==RANGE_COUNT) return STATUS_DEVICE_CONFIGURATION_ERROR;
        seen[j]=TRUE; found[j]=d->u.Memory.Start;
    }
    for (i=0;i<RANGE_COUNT;++i) if (!seen[i]) return STATUS_DEVICE_CONFIGURATION_ERROR;
    for (i=0;i<RANGE_COUNT;++i) {
        c->mmio[i]=MmMapIoSpace(found[i],RANGE_SIZE,MmNonCached);
        if (!c->mmio[i]) {
            while (i) { --i; MmUnmapIoSpace((PVOID)c->mmio[i],RANGE_SIZE); c->mmio[i]=NULL; }
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    return STATUS_SUCCESS;
}
NTSTATUS TouchRelease(WDFDEVICE device, WDFCMRESLIST translated)
{
    TOUCH_CONTEXT *c=TouchGetContext(device); unsigned i;
    UNREFERENCED_PARAMETER(translated);
    InterlockedExchange(&c->enabled,0);
    WdfTimerStop(c->timer,TRUE); WdfTimerStop(c->timer,TRUE);
    for (i=0;i<RANGE_COUNT;++i) if (c->mmio[i]) {
        MmUnmapIoSpace((PVOID)c->mmio[i],RANGE_SIZE); c->mmio[i]=NULL;
    }
    return STATUS_SUCCESS;
}
NTSTATUS TouchD0Entry(WDFDEVICE device, WDF_POWER_DEVICE_STATE previous)
{
    TOUCH_CONTEXT *c=TouchGetContext(device);
    struct a04_touch_board_io board={device,BoardUpdate,BoardWrite,BoardI2cWrite};
    struct a04_touch_calibration cal={0,4095,0,4095,0,0,0};
    struct a04_touch_pressure_policy pressure={730,305,5,900000};
    UNREFERENCED_PARAMETER(previous);
    if (c->faulted) return STATUS_DEVICE_HARDWARE_ERROR;
    c->bus.context=device; c->bus.read16=I2cRead; c->bus.write16=I2cWrite; c->bus.delay_us=Delay;
    c->adapter.bus=&c->bus; c->adapter.address=0x48; c->adapter.faulted=0;
    a04_touch_set_board_orientation(&cal);
    if (a04_touch_board_init(&board) || a04_bu21025_mt8167_bind(&c->adapter,&c->io) ||
        a04_touch_device_init(&c->touch,&c->io,&cal,&pressure)) {
        c->faulted=TRUE; return STATUS_DEVICE_HARDWARE_ERROR;
    }
    InterlockedExchange(&c->enabled,1);
    WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(20));
    return STATUS_SUCCESS;
}
NTSTATUS TouchD0Exit(WDFDEVICE device, WDF_POWER_DEVICE_STATE target)
{
    TOUCH_CONTEXT *c=TouchGetContext(device); UNREFERENCED_PARAMETER(target);
    InterlockedExchange(&c->enabled,0);
    WdfTimerStop(c->timer,TRUE); WdfTimerStop(c->timer,TRUE);
    return STATUS_SUCCESS;
}
static NTSTATUS CopyReport(WDFREQUEST request, const void *bytes, size_t length)
{
    WDFMEMORY memory; size_t n; NTSTATUS status;
    status=WdfRequestRetrieveOutputMemory(request,&memory);
    if (!NT_SUCCESS(status)) return status;
    WdfMemoryGetBuffer(memory,&n);
    if (n<length) return STATUS_BUFFER_TOO_SMALL;
    status=WdfMemoryCopyFromBuffer(memory,0,(PVOID)bytes,length);
    if (NT_SUCCESS(status)) WdfRequestSetInformation(request,length);
    return status;
}
void TouchTimer(WDFTIMER timer)
{
    WDFDEVICE device=(WDFDEVICE)WdfTimerGetParentObject(timer);
    TOUCH_CONTEXT *c=TouchGetContext(device);
    WDFREQUEST request;
    UCHAR report[A04_TOUCH_REPORT_SIZE];
    enum a04_touch_poll_result result;
    int down;
    ULONGLONG ticks, quotient=0, remainder=0; unsigned bit;
    if (!InterlockedCompareExchange(&c->enabled,1,1)) return;
    /* Do not advance contact state unless a HID read can consume the report. */
    if (!NT_SUCCESS(WdfIoQueueRetrieveNextRequest(c->reads,&request))) goto rearm;
    down=(READ_REGISTER_ULONG((volatile ULONG *)(c->mmio[GPIO]+0x200)) & (1u<<8)) == 0;
    ticks=KeQueryInterruptTime();
    /* 100 ns ticks -> 100 us HID scan units, avoiding ARM runtime division. */
    for (bit=0;bit<64;++bit) {
        remainder=(remainder<<1)|(ticks>>63); ticks<<=1; quotient<<=1;
        if (remainder>=1000) { remainder-=1000; quotient|=1; }
    }
    result=a04_touch_device_poll(&c->touch,down,(uint16_t)quotient,report);
    if (result==A04_POLL_REPORT || result==A04_POLL_FAULT_REPORT) {
        NTSTATUS status=CopyReport(request,report,sizeof(report));
        WdfRequestComplete(request,status);
    } else if (result==A04_POLL_FAULT) WdfRequestComplete(request,STATUS_DEVICE_HARDWARE_ERROR);
    else {
        NTSTATUS status=WdfRequestForwardToIoQueue(request,c->reads);
        if (!NT_SUCCESS(status)) WdfRequestComplete(request,status);
    }
    if (result==A04_POLL_FAULT || result==A04_POLL_FAULT_REPORT) {
        c->faulted=TRUE; InterlockedExchange(&c->enabled,0);
    }
rearm:
    if (InterlockedCompareExchange(&c->enabled,1,1)) WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(20));
}
void TouchIoctl(WDFQUEUE queue, WDFREQUEST request, size_t outlen, size_t inlen, ULONG code)
{
    TOUCH_CONTEXT *c=TouchGetContext(WdfIoQueueGetDevice(queue));
    HID_DESCRIPTOR hid={0}; HID_DEVICE_ATTRIBUTES attr={0}; NTSTATUS status=STATUS_NOT_SUPPORTED;
    UNREFERENCED_PARAMETER(inlen);
    switch (code) {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
        hid.bLength=sizeof(hid); hid.bDescriptorType=HID_HID_DESCRIPTOR_TYPE;
        hid.bcdHID=0x0111; hid.bNumDescriptors=1;
        hid.DescriptorList[0].bReportType=HID_REPORT_DESCRIPTOR_TYPE;
        hid.DescriptorList[0].wReportLength=(USHORT)c->descriptor_length;
        status=CopyReport(request,&hid,sizeof(hid)); break;
    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        attr.Size=sizeof(attr); attr.VendorID=0xffff; attr.ProductID=0x0004; attr.VersionNumber=1;
        status=CopyReport(request,&attr,sizeof(attr)); break;
    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        status=CopyReport(request,c->descriptor,c->descriptor_length); break;
    case IOCTL_HID_READ_REPORT:
        status=WdfRequestForwardToIoQueue(request,c->reads);
        if (NT_SUCCESS(status)) return;
        break;
    case IOCTL_HID_GET_FEATURE: {
        PIRP irp=WdfRequestWdmGetIrp(request);
        PHID_XFER_PACKET packet=(PHID_XFER_PACKET)irp->UserBuffer;
        if (outlen>=sizeof(*packet) && packet && packet->reportId==2 && packet->reportBuffer && packet->reportBufferLen>=A04_TOUCH_FEATURE_SIZE) {
            a04_touch_max_contacts(packet->reportBuffer);
            WdfRequestSetInformation(request,A04_TOUCH_FEATURE_SIZE); status=STATUS_SUCCESS;
        } else status=STATUS_INVALID_PARAMETER;
        break;
    }
    }
    WdfRequestComplete(request,status);
}
NTSTATUS TouchAdd(WDFDRIVER driver, PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp; WDF_OBJECT_ATTRIBUTES attr,timerattr;
    WDF_IO_QUEUE_CONFIG q; WDF_TIMER_CONFIG t; WDFDEVICE device; TOUCH_CONTEXT *c; NTSTATUS status;
    UNREFERENCED_PARAMETER(driver);
    WdfFdoInitSetFilter(init);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware=TouchPrepare; pnp.EvtDeviceReleaseHardware=TouchRelease;
    pnp.EvtDeviceD0Entry=TouchD0Entry; pnp.EvtDeviceD0Exit=TouchD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,TOUCH_CONTEXT);
    status=WdfDeviceCreate(&init,&attr,&device); if (!NT_SUCCESS(status)) return status;
    c=TouchGetContext(device); c->device=device;
    /* Approximate 10.1-inch 16:10 panel dimensions; replace when verified. */
    if (a04_touch_descriptor(535,856,c->descriptor,sizeof(c->descriptor),&c->descriptor_length))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchParallel);
    q.EvtIoInternalDeviceControl=TouchIoctl;
    status=WdfIoQueueCreate(device,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE); if (!NT_SUCCESS(status)) return status;
    WDF_IO_QUEUE_CONFIG_INIT(&q,WdfIoQueueDispatchManual);
    status=WdfIoQueueCreate(device,&q,WDF_NO_OBJECT_ATTRIBUTES,&c->reads); if (!NT_SUCCESS(status)) return status;
    WDF_TIMER_CONFIG_INIT(&t,TouchTimer); t.AutomaticSerialization=FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&timerattr); timerattr.ParentObject=device; timerattr.ExecutionLevel=WdfExecutionLevelPassive;
    return WdfTimerCreate(&t,&timerattr,&c->timer);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    WDF_DRIVER_CONFIG config; WDF_DRIVER_CONFIG_INIT(&config,TouchAdd);
    return WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
