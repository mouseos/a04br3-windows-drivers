/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental A04BR3 HID lower filter. It requires the opt-in MOU0004 ACPI
 * device and exclusive ownership of the declared MMIO ranges. Polling only;
 * does not support wake. Recovery resets only the exclusively owned I2C0.
 */
#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>
#include "board_init.h"
#include "device.h"
#include "mt8167_adapter.h"
#ifndef A04_TOUCH_SHARED_PARENT
#define A04_TOUCH_SHARED_PARENT 0
#endif
#if A04_TOUCH_SHARED_PARENT
#include "../../bus/mt8167-shared/client_interface.h"
#endif
#ifndef A04_TOUCH_THERMAL_LEASE
#define A04_TOUCH_THERMAL_LEASE 0
#endif
#if A04_TOUCH_SHARED_PARENT && A04_TOUCH_THERMAL_LEASE
#include "../../bus/mt8167-shared/thermal_interface.h"
#endif
#if A04_TOUCH_THERMAL_LEASE
#include "thermal_lease.h"
#include "thermal_broker.h"
#endif

#ifndef A04_TOUCH_TRACE
#define A04_TOUCH_TRACE 0
#endif

enum { TOP, INFRA, GPIO, I2C, RANGE_COUNT };
static const ULONG addresses[RANGE_COUNT] = {0x10000000,0x10001000,0x10005000,0x11009000};
#define RANGE_SIZE 0x1000u
/* Provisional model-wide geometry: 10.1-inch 16:10 display and the user's
 * reported approximately 10-mm inset. Not per-unit factory calibration. */
#define A04_PANEL_WIDTH_HUNDREDTH_IN 535u
#define A04_PANEL_HEIGHT_HUNDREDTH_IN 856u
#define A04_NOMINAL_INSET_HUNDREDTH_IN 39u

typedef struct {
    WDFDEVICE device;
#if A04_TOUCH_SHARED_PARENT
    A04_SHARED_INTERFACE shared;
#endif
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
#if A04_TOUCH_THERMAL_LEASE
    WDFSPINLOCK shared_lock;
    struct a04_thermal_lease thermal_lease;
    EX_RUNDOWN_REF thermal_rundown; BOOLEAN thermal_rundown_closed;
#if A04_TOUCH_SHARED_PARENT
    A04_SHARED_THERMAL_INTERFACE thermal_parent;ULONG thermal_parent_generation;
#endif
#endif
    BOOLEAN faulted;
    BOOLEAN recovering;
    unsigned recovery_attempts;
#if A04_TOUCH_TRACE
    volatile ULONG *uart;
    ULONG reports;
#endif
} TOUCH_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(TOUCH_CONTEXT, TouchGetContext)
EVT_WDF_DRIVER_DEVICE_ADD TouchAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE TouchPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE TouchRelease;
EVT_WDF_DEVICE_D0_ENTRY TouchD0Entry;
EVT_WDF_DEVICE_D0_EXIT TouchD0Exit;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL TouchIoctl;
EVT_WDF_TIMER TouchTimer;
#if A04_TOUCH_THERMAL_LEASE
EVT_WDF_FILE_CLEANUP TouchThermalCleanup;
static void TouchThermalOffline(TOUCH_CONTEXT *);
#endif

#if A04_TOUCH_TRACE
/* Diagnostic builds only; shares the UART with the existing monitor. */
static void Trace(TOUCH_CONTEXT *c, const char *s)
{
    ULONG n;
    if (!c->uart) return;
    while (*s) {
        for (n=100000;n;--n)
            if (!(READ_REGISTER_ULONG(c->uart+3)&0x80u) && (READ_REGISTER_ULONG(c->uart+5)&0x20u)) break;
        if (!n) return;
        WRITE_REGISTER_ULONG(c->uart,(ULONG)(UCHAR)*s++);
    }
}
static void TraceHex(TOUCH_CONTEXT *c, ULONG value)
{
    static const char digits[]="0123456789ABCDEF"; char s[9]; unsigned i;
    for (i=0;i<8;++i) s[i]=digits[(value>>(28-4*i))&15u];
    s[8]=0; Trace(c,s);
}
#else
#define Trace(c,s) ((void)0)
#endif

static uint16_t I2cRead(void *p, unsigned off)
{
    TOUCH_CONTEXT *c=TouchGetContext((WDFDEVICE)p);
    uint16_t value=READ_REGISTER_USHORT((volatile USHORT *)(c->mmio[I2C]+off));
#if A04_TOUCH_TRACE
    if (off==0x0c && (value & 6)) {
        Trace(c,"A04TOUCH I2C status="); TraceHex(c,value); Trace(c,"\n");
    }
#endif
    return value;
}
static void I2cWrite(void *p, unsigned off, uint16_t v)
{ WRITE_REGISTER_USHORT((volatile USHORT *)(TouchGetContext((WDFDEVICE)p)->mmio[I2C]+off), v); }
static void Delay(void *p, unsigned us)
{ UNREFERENCED_PARAMETER(p); KeStallExecutionProcessor(us); }
static int BoardUpdate(void *p, enum a04_touch_register_bank bank, unsigned off, uint32_t mask, uint32_t bits)
{
    TOUCH_CONTEXT *c = TouchGetContext((WDFDEVICE)p);
#if A04_TOUCH_SHARED_PARENT
    int result=c->shared.TouchUpdate(c->shared.Context,(unsigned)bank,off,mask,bits);
#if A04_TOUCH_TRACE
    if (result) { Trace(c,"A04TOUCH update failed bank="); TraceHex(c,bank);
        Trace(c," offset="); TraceHex(c,off); Trace(c,"\n"); }
#endif
    return result;
#else
    unsigned i = bank == A04_TOUCH_GPIO ? GPIO : bank == A04_TOUCH_INFRA ? INFRA : TOP;
    volatile ULONG *reg;
    ULONG value;
    if (!c->mmio[i] || off > RANGE_SIZE-4 || (bits & ~mask)) return -1;
    reg = (volatile ULONG *)(c->mmio[i]+off);
#if A04_TOUCH_THERMAL_LEASE
    WdfSpinLockAcquire(c->shared_lock);
#endif
    value = READ_REGISTER_ULONG(reg);
    WRITE_REGISTER_ULONG(reg, (value & ~mask) | bits);
    value = READ_REGISTER_ULONG(reg);
#if A04_TOUCH_THERMAL_LEASE
    WdfSpinLockRelease(c->shared_lock);
#endif
    return (value & mask) == bits ? 0 : -1;
#endif
}
static int BoardWrite(void *p, enum a04_touch_register_bank bank, unsigned off, uint32_t value)
{
    TOUCH_CONTEXT *c = TouchGetContext((WDFDEVICE)p);
#if A04_TOUCH_SHARED_PARENT
    int result=c->shared.TouchWrite(c->shared.Context,(unsigned)bank,off,value);
#if A04_TOUCH_TRACE
    if (result) { Trace(c,"A04TOUCH write failed bank="); TraceHex(c,bank);
        Trace(c," offset="); TraceHex(c,off); Trace(c,"\n"); }
#endif
    return result;
#else
    unsigned i = bank == A04_TOUCH_GPIO ? GPIO : TOP;
    if (!c->mmio[i] || off > RANGE_SIZE-4) return -1;
#if A04_TOUCH_THERMAL_LEASE
    WdfSpinLockAcquire(c->shared_lock);
#endif
    WRITE_REGISTER_ULONG((volatile ULONG *)(c->mmio[i]+off), value);
#if A04_TOUCH_THERMAL_LEASE
    WdfSpinLockRelease(c->shared_lock);
#endif
    return 0;
#endif
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
#if A04_TOUCH_SHARED_PARENT
    if (seen[TOP] || seen[INFRA] || seen[GPIO] || !seen[I2C]) return STATUS_DEVICE_CONFIGURATION_ERROR;
    {
        NTSTATUS status=WdfFdoQueryForInterface(device,&A04SharedInterfaceGuid,
            (PINTERFACE)&c->shared,sizeof(c->shared),A04_SHARED_INTERFACE_VERSION,NULL);
        if (!NT_SUCCESS(status)) return status;
        if (!c->shared.InterfaceDereference || !c->shared.TouchUpdate || !c->shared.TouchWrite || !c->shared.TouchPen) {
            if(c->shared.InterfaceDereference)c->shared.InterfaceDereference(c->shared.Context);
            RtlZeroMemory(&c->shared,sizeof(c->shared));return STATUS_NOINTERFACE;
        }
    }
#if A04_TOUCH_THERMAL_LEASE
    {
        NTSTATUS status=WdfFdoQueryForInterface(device,&A04SharedThermalInterfaceGuid,
            (PINTERFACE)&c->thermal_parent,sizeof(c->thermal_parent),A04_SHARED_THERMAL_INTERFACE_VERSION,NULL);
        if(!NT_SUCCESS(status)) {c->shared.InterfaceDereference(c->shared.Context);RtlZeroMemory(&c->shared,sizeof(c->shared));return status;}
        if(!c->thermal_parent.Header.InterfaceDereference || !c->thermal_parent.Acquire || !c->thermal_parent.Clock ||
           !c->thermal_parent.Reset || !c->thermal_parent.Begin || !c->thermal_parent.End || !c->thermal_parent.ReadAnalog || !c->thermal_parent.WriteAnalog || !c->thermal_parent.Quiescent || !c->thermal_parent.Release || !c->thermal_parent.VerifyHeadphone) {
            if(c->thermal_parent.Header.InterfaceDereference)c->thermal_parent.Header.InterfaceDereference(c->thermal_parent.Header.Context);
            RtlZeroMemory(&c->thermal_parent,sizeof(c->thermal_parent));c->shared.InterfaceDereference(c->shared.Context);
            RtlZeroMemory(&c->shared,sizeof(c->shared));return STATUS_NOINTERFACE;
        }
    }
#endif
#else
    for (i=0;i<RANGE_COUNT;++i) if (!seen[i]) return STATUS_DEVICE_CONFIGURATION_ERROR;
#endif
    for (i=0;i<RANGE_COUNT;++i) {
#if A04_TOUCH_SHARED_PARENT
        if (i!=I2C) continue;
#endif
        c->mmio[i]=MmMapIoSpace(found[i],RANGE_SIZE,MmNonCached);
        if (!c->mmio[i]) {
            while (i) { --i; if(c->mmio[i]) MmUnmapIoSpace((PVOID)c->mmio[i],RANGE_SIZE); c->mmio[i]=NULL; }
#if A04_TOUCH_SHARED_PARENT
#if A04_TOUCH_THERMAL_LEASE
            c->thermal_parent.Header.InterfaceDereference(c->thermal_parent.Header.Context);RtlZeroMemory(&c->thermal_parent,sizeof(c->thermal_parent));
#endif
            c->shared.InterfaceDereference(c->shared.Context); RtlZeroMemory(&c->shared,sizeof(c->shared));
#endif
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }
#if A04_TOUCH_TRACE
    { PHYSICAL_ADDRESS uart; uart.QuadPart=0x11005000u;
      c->uart=MmMapIoSpace(uart,RANGE_SIZE,MmNonCached); }
#endif
    Trace(c,"A04TOUCH prepare complete\n");
    return STATUS_SUCCESS;
}
NTSTATUS TouchRelease(WDFDEVICE device, WDFCMRESLIST translated)
{
    TOUCH_CONTEXT *c=TouchGetContext(device); unsigned i;
    UNREFERENCED_PARAMETER(translated);
    InterlockedExchange(&c->enabled,0);
#if A04_TOUCH_THERMAL_LEASE
    /* Close admission and drain direct thermal MMIO before invalidating the
     * lease or unmapping. Do not hold the shared spin lock while waiting. */
    if(!c->thermal_rundown_closed) {
        ExWaitForRundownProtectionRelease(&c->thermal_rundown);
        c->thermal_rundown_closed=TRUE;
    }
    TouchThermalOffline(c);
#endif
    WdfTimerStop(c->timer,TRUE); WdfTimerStop(c->timer,TRUE);
    for (i=0;i<RANGE_COUNT;++i) if (c->mmio[i]) {
        MmUnmapIoSpace((PVOID)c->mmio[i],RANGE_SIZE); c->mmio[i]=NULL;
    }
#if A04_TOUCH_SHARED_PARENT
#if A04_TOUCH_THERMAL_LEASE
    if(c->thermal_parent.Header.InterfaceDereference) {c->thermal_parent.Header.InterfaceDereference(c->thermal_parent.Header.Context);RtlZeroMemory(&c->thermal_parent,sizeof(c->thermal_parent));}
#endif
    if(c->shared.InterfaceDereference) { c->shared.InterfaceDereference(c->shared.Context); RtlZeroMemory(&c->shared,sizeof(c->shared)); }
#endif
#if A04_TOUCH_TRACE
    Trace(c,"A04TOUCH release\n");
    if (c->uart) { MmUnmapIoSpace((PVOID)c->uart,RANGE_SIZE); c->uart=NULL; }
#endif
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
    Trace(c,"A04TOUCH D0 entry\n");
    c->bus.context=device; c->bus.read16=I2cRead; c->bus.write16=I2cWrite; c->bus.delay_us=Delay;
    c->adapter.bus=&c->bus; c->adapter.address=0x48; c->adapter.faulted=0;
    a04_touch_set_board_orientation(&cal);
    if (a04_touch_set_nominal_inset(&cal,A04_PANEL_WIDTH_HUNDREDTH_IN,
                                     A04_PANEL_HEIGHT_HUNDREDTH_IN,A04_NOMINAL_INSET_HUNDREDTH_IN))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    if (a04_touch_board_init(&board)) {
        Trace(c,"A04TOUCH board init failed\n");
        c->faulted=TRUE; return STATUS_DEVICE_HARDWARE_ERROR;
    }
    if (a04_bu21025_mt8167_bind(&c->adapter,&c->io)) {
        Trace(c,"A04TOUCH adapter bind failed\n");
        c->faulted=TRUE; return STATUS_DEVICE_HARDWARE_ERROR;
    }
    if (a04_touch_device_init(&c->touch,&c->io,&cal,&pressure)) {
        Trace(c,"A04TOUCH controller init failed\n");
        c->faulted=TRUE; return STATUS_DEVICE_HARDWARE_ERROR;
    }
    c->recovering=FALSE; c->recovery_attempts=0;
    Trace(c,"A04TOUCH hardware init success\n");
#if A04_TOUCH_THERMAL_LEASE
    if(c->thermal_rundown_closed) {
        ExReInitializeRundownProtection(&c->thermal_rundown);
        c->thermal_rundown_closed=FALSE;
    }
    WdfSpinLockAcquire(c->shared_lock);
    c->thermal_lease.online=1;
    WdfSpinLockRelease(c->shared_lock);
#endif
    InterlockedExchange(&c->enabled,1);
    WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(20));
    return STATUS_SUCCESS;
}
NTSTATUS TouchD0Exit(WDFDEVICE device, WDF_POWER_DEVICE_STATE target)
{
    TOUCH_CONTEXT *c=TouchGetContext(device); UNREFERENCED_PARAMETER(target);
    InterlockedExchange(&c->enabled,0);
#if A04_TOUCH_THERMAL_LEASE
    /* Close admission and drain direct thermal MMIO before invalidating the
     * lease or unmapping. Do not hold the shared spin lock while waiting. */
    if(!c->thermal_rundown_closed) {
        ExWaitForRundownProtectionRelease(&c->thermal_rundown);
        c->thermal_rundown_closed=TRUE;
    }
    TouchThermalOffline(c);
#endif
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
    ULONG delay=20;
    ULONGLONG ticks, quotient=0, remainder=0; unsigned bit;
    if (!InterlockedCompareExchange(&c->enabled,1,1)) return;
    if (c->recovering) {
        struct a04_touch_board_io board={device,BoardUpdate,BoardWrite,BoardI2cWrite};
        struct a04_touch_calibration cal={0,4095,0,4095,0,0,0};
        struct a04_touch_pressure_policy pressure={730,305,5,900000};
        if (c->recovery_attempts>=3) {
            c->faulted=TRUE; InterlockedExchange(&c->enabled,0);
            Trace(c,"A04TOUCH recover exhausted\n");
            goto rearm;
        }
        a04_touch_set_board_orientation(&cal);
        if (a04_touch_set_nominal_inset(&cal,A04_PANEL_WIDTH_HUNDREDTH_IN,
                                        A04_PANEL_HEIGHT_HUNDREDTH_IN,A04_NOMINAL_INSET_HUNDREDTH_IN)) {
            c->faulted=TRUE; InterlockedExchange(&c->enabled,0); goto rearm;
        }
        ++c->recovery_attempts;
        Trace(c,"A04TOUCH recover attempt\n");
        if (!a04_touch_i2c_reset(&board)) {
            c->adapter.faulted=0;
            if (!a04_bu21025_mt8167_bind(&c->adapter,&c->io) &&
                !a04_touch_device_init(&c->touch,&c->io,&cal,&pressure)) {
                c->recovering=FALSE;
                Trace(c,"A04TOUCH recover success\n");
                goto rearm;
            }
        }
        if (c->recovery_attempts>=3) {
            c->faulted=TRUE; InterlockedExchange(&c->enabled,0);
            Trace(c,"A04TOUCH recover exhausted\n");
        }
        delay=250;
        goto rearm;
    }
    /* Do not advance contact state unless a HID read can consume the report. */
    if (!NT_SUCCESS(WdfIoQueueRetrieveNextRequest(c->reads,&request))) goto rearm;
#if A04_TOUCH_SHARED_PARENT
    if (c->shared.TouchPen(c->shared.Context,&down)) {
        WdfRequestComplete(request,STATUS_DEVICE_NOT_READY);
        c->faulted=TRUE; InterlockedExchange(&c->enabled,0); goto rearm;
    }
#else
    down=(READ_REGISTER_ULONG((volatile ULONG *)(c->mmio[GPIO]+0x200)) & (1u<<8)) == 0;
#endif
    ticks=KeQueryInterruptTime();
    /* 100 ns ticks -> 100 us HID scan units, avoiding ARM runtime division. */
    for (bit=0;bit<64;++bit) {
        remainder=(remainder<<1)|(ticks>>63); ticks<<=1; quotient<<=1;
        if (remainder>=1000) { remainder-=1000; quotient|=1; }
    }
    result=a04_touch_device_poll(&c->touch,down,(uint16_t)quotient,report);
    if (result==A04_POLL_REPORT || result==A04_POLL_FAULT_REPORT) {
        NTSTATUS status=CopyReport(request,report,sizeof(report));
#if A04_TOUCH_TRACE
        if (c->reports++<32) {
            Trace(c,"A04TOUCH report down="); TraceHex(c,report[1]);
            Trace(c," x="); TraceHex(c,report[3]|((ULONG)report[4]<<8));
            Trace(c," y="); TraceHex(c,report[5]|((ULONG)report[6]<<8));
            Trace(c," status="); TraceHex(c,(ULONG)status); Trace(c,"\n");
        }
#endif
        WdfRequestComplete(request,status);
        if (!NT_SUCCESS(status)) {
            c->faulted=TRUE; InterlockedExchange(&c->enabled,0);
            Trace(c,"A04TOUCH report delivery failed\n");
            goto rearm;
        }
        if (result==A04_POLL_REPORT) c->recovery_attempts=0;
    } else {
        NTSTATUS status=WdfRequestForwardToIoQueue(request,c->reads);
        if (!NT_SUCCESS(status)) WdfRequestComplete(request,status);
    }
    if (result==A04_POLL_FAULT || result==A04_POLL_FAULT_REPORT) {
        Trace(c,"A04TOUCH poll fault\n");
        c->recovering=TRUE; delay=250;
    }
rearm:
    if (InterlockedCompareExchange(&c->enabled,1,1)) WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(delay));
}
#if A04_TOUCH_THERMAL_LEASE
static void TouchThermalOffline(TOUCH_CONTEXT *c)
{
#if A04_TOUCH_SHARED_PARENT
    ULONG generation;
#endif
    WdfSpinLockAcquire(c->shared_lock);
#if A04_TOUCH_SHARED_PARENT
    generation=c->thermal_parent_generation;c->thermal_parent_generation=0;
#endif
    a04_lease_offline(&c->thermal_lease);WdfSpinLockRelease(c->shared_lock);
#if A04_TOUCH_SHARED_PARENT
    if(generation && c->thermal_parent.Release)(VOID)c->thermal_parent.Release(c->thermal_parent.Header.Context,generation);
#endif
}
VOID TouchThermalCleanup(WDFFILEOBJECT file)
{
    TOUCH_CONTEXT *c=TouchGetContext(WdfFileObjectGetDevice(file));
    uintptr_t owner=(uintptr_t)WdfFileObjectWdmGetFileObject(file);
#if A04_TOUCH_SHARED_PARENT
    ULONG generation=0;
#endif
    WdfSpinLockAcquire(c->shared_lock);
#if A04_TOUCH_SHARED_PARENT
    if(a04_lease_valid(&c->thermal_lease,owner)){generation=c->thermal_parent_generation;c->thermal_parent_generation=0;}
#endif
    a04_lease_release(&c->thermal_lease,owner);WdfSpinLockRelease(c->shared_lock);
#if A04_TOUCH_SHARED_PARENT
    if(generation && c->thermal_parent.Release)(VOID)c->thermal_parent.Release(c->thermal_parent.Header.Context,generation);
#endif
}
static BOOLEAN TouchThermalGuardBegin(PVOID context,ULONG_PTR owner,ULONG generation)
{
    WDFDEVICE device=(WDFDEVICE)context;TOUCH_CONTEXT *c=TouchGetContext(device);BOOLEAN valid;
    if(!ExAcquireRundownProtection(&c->thermal_rundown))return FALSE;
    WdfSpinLockAcquire(c->shared_lock);
    valid=a04_lease_guard_begin(&c->thermal_lease,(uintptr_t)owner,generation)?FALSE:TRUE;
    WdfSpinLockRelease(c->shared_lock);
#if A04_TOUCH_SHARED_PARENT
    if(valid && (!c->thermal_parent.Begin || !c->thermal_parent.Begin(c->thermal_parent.Header.Context,c->thermal_parent_generation))) {
        WdfSpinLockAcquire(c->shared_lock);a04_lease_guard_end(&c->thermal_lease);WdfSpinLockRelease(c->shared_lock);valid=FALSE;
    }
#endif
    if(!valid)ExReleaseRundownProtection(&c->thermal_rundown);
    return valid;
}
static VOID TouchThermalGuardEnd(PVOID context)
{
    TOUCH_CONTEXT *c=TouchGetContext((WDFDEVICE)context);
#if A04_TOUCH_SHARED_PARENT
    c->thermal_parent.End(c->thermal_parent.Header.Context);
#endif
    WdfSpinLockAcquire(c->shared_lock);a04_lease_guard_end(&c->thermal_lease);WdfSpinLockRelease(c->shared_lock);
    ExReleaseRundownProtection(&c->thermal_rundown);
}
#if A04_TOUCH_SHARED_PARENT
static NTSTATUS TouchThermalAnalogRead(PVOID context,ULONG *value)
{
 TOUCH_CONTEXT *c=TouchGetContext((WDFDEVICE)context);
 return c->thermal_parent.ReadAnalog(c->thermal_parent.Header.Context,c->thermal_parent_generation,value);
}
static NTSTATUS TouchThermalAnalogWrite(PVOID context,ULONG value)
{
 TOUCH_CONTEXT *c=TouchGetContext((WDFDEVICE)context);
 return c->thermal_parent.WriteAnalog(c->thermal_parent.Header.Context,c->thermal_parent_generation,value);
}
static NTSTATUS TouchThermalVerifyHeadphone(PVOID context)
{
 TOUCH_CONTEXT *c=TouchGetContext((WDFDEVICE)context);
 return c->thermal_parent.VerifyHeadphone(c->thermal_parent.Header.Context,c->thermal_parent_generation);
}
#endif
static VOID TouchThermalGuardRelease(PVOID context)
{ WdfObjectDereference((WDFDEVICE)context); }
static NTSTATUS TouchThermalRequest(TOUCH_CONTEXT *c,WDFREQUEST request,ULONG code)
{
    PIRP irp=WdfRequestWdmGetIrp(request);
    uintptr_t owner=(uintptr_t)IoGetCurrentIrpStackLocation(irp)->FileObject;
    NTSTATUS status=STATUS_DEVICE_NOT_READY; ULONG state;A04_TOUCH_THERMAL_GUARD *guard;size_t output;
    if(irp->RequestorMode!=KernelMode || !owner) return STATUS_ACCESS_DENIED;
#if A04_TOUCH_SHARED_PARENT
    if(code==A04_TOUCH_THERMAL_CLAIM) {
        ULONG gen=0;status=c->thermal_parent.Acquire(c->thermal_parent.Header.Context,&gen);
        if(!NT_SUCCESS(status))return status;
        WdfSpinLockAcquire(c->shared_lock);
        if(a04_lease_claim(&c->thermal_lease,owner))status=STATUS_DEVICE_BUSY;
        else {c->thermal_parent_generation=gen;status=STATUS_SUCCESS;}
        WdfSpinLockRelease(c->shared_lock);
        if(!NT_SUCCESS(status))(VOID)c->thermal_parent.Release(c->thermal_parent.Header.Context,gen);
        return status;
    }
    if(code==A04_TOUCH_THERMAL_RELEASE || code==A04_TOUCH_THERMAL_CLOCK || code==A04_TOUCH_THERMAL_RESET || code==A04_TOUCH_THERMAL_QUIESCENT) {
        ULONG gen=0;
        WdfSpinLockAcquire(c->shared_lock);
        if(a04_lease_valid(&c->thermal_lease,owner)) {
            gen=c->thermal_parent_generation;
            if(code==A04_TOUCH_THERMAL_RELEASE){a04_lease_release(&c->thermal_lease,owner);c->thermal_parent_generation=0;}
        }
        WdfSpinLockRelease(c->shared_lock);if(!gen)return STATUS_DEVICE_NOT_READY;
        if(code==A04_TOUCH_THERMAL_RELEASE)return c->thermal_parent.Release(c->thermal_parent.Header.Context,gen);
        if(code==A04_TOUCH_THERMAL_QUIESCENT)return c->thermal_parent.Quiescent(c->thermal_parent.Header.Context,gen);
        if(code==A04_TOUCH_THERMAL_CLOCK)return c->thermal_parent.Clock(c->thermal_parent.Header.Context,gen);
        return c->thermal_parent.Reset(c->thermal_parent.Header.Context,gen);
    }
#endif
    WdfSpinLockAcquire(c->shared_lock);
    if(code==A04_TOUCH_THERMAL_CLAIM) {
        status=a04_lease_claim(&c->thermal_lease,owner) ? STATUS_DEVICE_BUSY : STATUS_SUCCESS;
    } else if(a04_lease_valid(&c->thermal_lease,owner)) {
        if(code==A04_TOUCH_THERMAL_GET_GUARD) {
            status=WdfRequestRetrieveOutputBuffer(request,sizeof(*guard),(PVOID *)&guard,&output);
            if(NT_SUCCESS(status)) {
                WdfObjectReference(c->device);guard->size=sizeof(*guard);guard->version=2;
                guard->context=c->device;guard->owner=(ULONG_PTR)owner;guard->generation=c->thermal_lease.generation;
#if A04_TOUCH_SHARED_PARENT
                guard->ReadAnalog=TouchThermalAnalogRead;guard->WriteAnalog=TouchThermalAnalogWrite;
                guard->VerifyHeadphone=TouchThermalVerifyHeadphone;
#else
                guard->ReadAnalog=NULL;guard->WriteAnalog=NULL;
                guard->VerifyHeadphone=NULL;
#endif
                guard->Begin=TouchThermalGuardBegin;guard->End=TouchThermalGuardEnd;
                guard->Release=TouchThermalGuardRelease;WdfRequestSetInformation(request,sizeof(*guard));
            }
        } else if(code==A04_TOUCH_THERMAL_QUERY) {
            status=STATUS_SUCCESS;
        } else if(code==A04_TOUCH_THERMAL_CLOCK) {
            /* CLK_TOP_THEM bit 1 and CLK_TOP_AUX_ADC bit 30: set/clear alias. */
            WRITE_REGISTER_ULONG((volatile ULONG *)(c->mmio[TOP]+0x84),(1u<<1)|(1u<<30));
            state=READ_REGISTER_ULONG((volatile ULONG *)(c->mmio[TOP]+0x24));
            if(!(state & ((1u<<1)|(1u<<30)))) { c->thermal_lease.clocks_ready=1; status=STATUS_SUCCESS; }
            else status=STATUS_DEVICE_HARDWARE_ERROR;
        } else if(code==A04_TOUCH_THERMAL_RESET && c->thermal_lease.clocks_ready) {
            /* Atomic set/clear bit 0 only; never reset the touch controller. */
            WRITE_REGISTER_ULONG((volatile ULONG *)(c->mmio[INFRA]+0x120),1u);
            WRITE_REGISTER_ULONG((volatile ULONG *)(c->mmio[INFRA]+0x124),1u);
            state=READ_REGISTER_ULONG((volatile ULONG *)(c->mmio[INFRA]+0x128));
            status=(state & 1u)?STATUS_DEVICE_HARDWARE_ERROR:STATUS_SUCCESS;
        } else if(code==A04_TOUCH_THERMAL_RELEASE) {
            a04_lease_release(&c->thermal_lease,owner); status=STATUS_SUCCESS;
        }
    }
    WdfSpinLockRelease(c->shared_lock); return status;
}
#endif
void TouchIoctl(WDFQUEUE queue, WDFREQUEST request, size_t outlen, size_t inlen, ULONG code)
{
    TOUCH_CONTEXT *c=TouchGetContext(WdfIoQueueGetDevice(queue));
    HID_DESCRIPTOR hid={0}; HID_DEVICE_ATTRIBUTES attr={0}; NTSTATUS status=STATUS_NOT_SUPPORTED;
    UNREFERENCED_PARAMETER(inlen);
#if A04_TOUCH_THERMAL_LEASE
    if(code==A04_TOUCH_THERMAL_CLAIM || code==A04_TOUCH_THERMAL_CLOCK ||
       code==A04_TOUCH_THERMAL_RESET || code==A04_TOUCH_THERMAL_RELEASE || code==A04_TOUCH_THERMAL_QUERY || code==A04_TOUCH_THERMAL_GET_GUARD || code==A04_TOUCH_THERMAL_QUIESCENT) {
        WdfRequestComplete(request,TouchThermalRequest(c,request,code)); return;
    }
#endif
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
#if A04_TOUCH_THERMAL_LEASE
    { DECLARE_CONST_UNICODE_STRING(name,A04_TOUCH_THERMAL_DEVICE); WDF_FILEOBJECT_CONFIG fc;
      status=WdfDeviceInitAssignName(init,&name); if(!NT_SUCCESS(status)) return status;
      WDF_FILEOBJECT_CONFIG_INIT(&fc,WDF_NO_EVENT_CALLBACK,WDF_NO_EVENT_CALLBACK,TouchThermalCleanup);
      WdfDeviceInitSetFileObjectConfig(init,&fc,WDF_NO_OBJECT_ATTRIBUTES); }
#endif
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware=TouchPrepare; pnp.EvtDeviceReleaseHardware=TouchRelease;
    pnp.EvtDeviceD0Entry=TouchD0Entry; pnp.EvtDeviceD0Exit=TouchD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,TOUCH_CONTEXT);
    status=WdfDeviceCreate(&init,&attr,&device); if (!NT_SUCCESS(status)) return status;
    c=TouchGetContext(device); c->device=device;
#if A04_TOUCH_THERMAL_LEASE
    ExInitializeRundownProtection(&c->thermal_rundown);
#endif
#if A04_TOUCH_THERMAL_LEASE
    { WDF_OBJECT_ATTRIBUTES lockattr; WDF_OBJECT_ATTRIBUTES_INIT(&lockattr); lockattr.ParentObject=device;
      status=WdfSpinLockCreate(&lockattr,&c->shared_lock); if(!NT_SUCCESS(status)) return status; }
#endif
    /* Approximate 10.1-inch 16:10 panel dimensions; replace when verified. */
    if (a04_touch_descriptor(A04_PANEL_WIDTH_HUNDREDTH_IN,A04_PANEL_HEIGHT_HUNDREDTH_IN,
                             c->descriptor,sizeof(c->descriptor),&c->descriptor_length))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchParallel);
    q.EvtIoInternalDeviceControl=TouchIoctl;
#if A04_TOUCH_SHARED_PARENT && A04_TOUCH_THERMAL_LEASE
    { WDF_OBJECT_ATTRIBUTES qa;WDF_OBJECT_ATTRIBUTES_INIT(&qa);qa.ExecutionLevel=WdfExecutionLevelPassive;
      status=WdfIoQueueCreate(device,&q,&qa,WDF_NO_HANDLE); }
#else
    status=WdfIoQueueCreate(device,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);
#endif
    if (!NT_SUCCESS(status)) return status;
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
