/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 TOPRGU handoff prototype. Hardware ownership and sleep are not yet
 * validated; do not deploy as the only recovery path without a rollback. */
#include <ntddk.h>
#include <wdf.h>
#include "reset_command.h"
#include "watchdog_policy.h"
#include "thermal_stop.h"
#ifndef A04_TOPRGU_THERMAL_STOP
#define A04_TOPRGU_THERMAL_STOP 0
#endif
#ifndef A04_TOPRGU_DEADLINE_SECONDS
#define A04_TOPRGU_DEADLINE_SECONDS 0
#endif

typedef struct {
    volatile ULONG *wdt, *uart;
    WDFTIMER timer;
    KTIMER backupTimer;
    KDPC backupDpc;
    volatile LONG running, busy;
    volatile LONG ownedFour;
#if A04_TOPRGU_THERMAL_STOP
    volatile LONG thermalHalt;
#endif
    BOOLEAN suspendedWatchdog;
    ULONG savedMode;
    ULONG ticks;
    ULONGLONG started;
    BOOLEAN deadlineReported;
    ULONG backupCalls;
    struct mt8167_reset_parser parser;
} TOPRGU_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(TOPRGU_CONTEXT, ToprguGetContext)
EVT_WDF_DRIVER_DEVICE_ADD ToprguAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE ToprguPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE ToprguRelease;
EVT_WDF_DEVICE_D0_ENTRY ToprguD0Entry;
EVT_WDF_DEVICE_D0_EXIT ToprguD0Exit;
EVT_WDF_TIMER ToprguTimer;
KDEFERRED_ROUTINE ToprguBackupDpc;
#if A04_TOPRGU_THERMAL_STOP
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL ToprguThermalControl;
#endif

static void Trace(TOPRGU_CONTEXT *c, const char *s)
{
    ULONG budget;
    if (!c->uart) return;
    while (*s) {
        for (budget=100000; budget; --budget)
            if (!(READ_REGISTER_ULONG(c->uart + 3) & 0x80u) &&
                (READ_REGISTER_ULONG(c->uart + 5) & 0x20u)) break;
        if (!budget) return;
        WRITE_REGISTER_ULONG(c->uart, (UCHAR)*s++);
    }
}

static void TraceTimer(TOPRGU_CONTEXT *c, ULONG ticks, ULONGLONG elapsed)
{
    static const char hex[]="0123456789ABCDEF";
    char message[]="A04RGU tick=00000000 cpu=00000000 elapsed_ms=00000000\n";
    ULONG values[3], i, j;
    values[0]=ticks;
    values[1]=KeGetCurrentProcessorNumber();
    /* Low 32 bits wrap after about 429 seconds; the deadline uses full time. */
    values[2]=(ULONG)elapsed/10000u;
    for (i=0; i<3; ++i) {
        ULONG offset=i==0 ? 12 : (i==1 ? 25 : 45);
        for (j=0; j<8; ++j) message[offset+j]=hex[(values[i] >> (28-4*j)) & 15u];
    }
    Trace(c,message);
}

NTSTATUS ToprguPrepare(WDFDEVICE device, WDFCMRESLIST raw, WDFCMRESLIST translated)
{
    TOPRGU_CONTEXT *c = ToprguGetContext(device);
    ULONG i, count = WdfCmResourceListGetCount(translated);
    UNREFERENCED_PARAMETER(raw);
    for (i=0; i<count; ++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(translated,i);
        if (!d || d->Type != CmResourceTypeMemory || d->u.Memory.Length < 0x1000) continue;
        if (d->u.Memory.Start.QuadPart == 0x10007000 && !c->wdt)
            c->wdt = MmMapIoSpace(d->u.Memory.Start,0x1000,MmNonCached);
        else if (d->u.Memory.Start.QuadPart == 0x11005000 && !c->uart)
            c->uart = MmMapIoSpace(d->u.Memory.Start,0x1000,MmNonCached);
    }
    if (!c->wdt || !c->uart) {
        if (c->wdt) { MmUnmapIoSpace((PVOID)c->wdt,0x1000); c->wdt=NULL; }
        if (c->uart) { MmUnmapIoSpace((PVOID)c->uart,0x1000); c->uart=NULL; }
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    return STATUS_SUCCESS;
}

NTSTATUS ToprguRelease(WDFDEVICE device, WDFCMRESLIST translated)
{
    TOPRGU_CONTEXT *c=ToprguGetContext(device);
    UNREFERENCED_PARAMETER(translated);
    WdfTimerStop(c->timer,TRUE);
    if (c->uart) { MmUnmapIoSpace((PVOID)c->uart,0x1000); c->uart=NULL; }
    if (c->wdt) { MmUnmapIoSpace((PVOID)c->wdt,0x1000); c->wdt=NULL; }
    return STATUS_SUCCESS;
}

NTSTATUS ToprguD0Entry(WDFDEVICE device, WDF_POWER_DEVICE_STATE previous)
{
    TOPRGU_CONTEXT *c=ToprguGetContext(device);
    UNREFERENCED_PARAMETER(previous);
    if (!c->wdt) return STATUS_DEVICE_NOT_READY;
#if A04_TOPRGU_THERMAL_STOP
    if (InterlockedCompareExchange(&c->thermalHalt,0,0)) return STATUS_DEVICE_NOT_READY;
#endif
    if (c->suspendedWatchdog) {
        WRITE_REGISTER_ULONG(c->wdt + 1,(31u << 11) | 0x08u);
        WRITE_REGISTER_ULONG(c->wdt + 2,0x1971u);
        WRITE_REGISTER_ULONG(c->wdt,(c->savedMode & 0x00ffffffu & ~0x48u) | 0x22000015u);
        if (!(READ_REGISTER_ULONG(c->wdt) & 1u)) return STATUS_DEVICE_NOT_READY;
        c->suspendedWatchdog=FALSE;
        Trace(c,"A04RGU watchdog rearmed after power transition\n");
    }
    if (!(READ_REGISTER_ULONG(c->wdt) & 1u)) return STATUS_DEVICE_NOT_READY;
    WRITE_REGISTER_ULONG(c->wdt + 2,0x1971u);
    c->parser.matched=0; c->ticks=0;
    c->started=KeQueryInterruptTime(); c->deadlineReported=FALSE;
    c->backupCalls=0;
    InterlockedExchange(&c->running,1);
    Trace(c, KeQueryActiveProcessorCount(NULL) >= 4 ?
          "A04RGU D0 active CPUs>=4\n" : "A04RGU D0 active CPUs<4\n");
#if A04_TOPRGU_DEADLINE_SECONDS > 0
    Trace(c,"A04RGU bounded watchdog test enabled\n");
#endif
    WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(20));
    if (KeQueryActiveProcessorCount(NULL) >= 4) {
        LARGE_INTEGER due;
        due.QuadPart=-200000; /* 20 ms in 100 ns units. */
        KeSetTargetProcessorDpc(&c->backupDpc,2);
        KeSetTimerEx(&c->backupTimer,due,20,&c->backupDpc);
    }
    return STATUS_SUCCESS;
}

NTSTATUS ToprguD0Exit(WDFDEVICE device, WDF_POWER_DEVICE_STATE target)
{
    TOPRGU_CONTEXT *c=ToprguGetContext(device);
    POWER_ACTION action=WdfDeviceGetSystemPowerAction(device);
    UNREFERENCED_PARAMETER(target);
    InterlockedExchange(&c->running,0);
    KeCancelTimer(&c->backupTimer);
    KeFlushQueuedDpcs();
    WdfTimerStop(c->timer,TRUE);
    if (InterlockedCompareExchange(&c->ownedFour,1,1) && c->wdt &&
#if A04_TOPRGU_THERMAL_STOP
        !InterlockedCompareExchange(&c->thermalHalt,0,0) &&
#endif
        (action==PowerActionSleep || action==PowerActionHibernate ||
         action==PowerActionShutdown || action==PowerActionShutdownOff)) {
        c->savedMode=READ_REGISTER_ULONG(c->wdt) & 0x00ffffffu;
        WRITE_REGISTER_ULONG(c->wdt,(c->savedMode & ~1u) | 0x22000000u);
        c->suspendedWatchdog=TRUE;
        Trace(c,"A04RGU watchdog disabled for system power transition\n");
    }
    return STATUS_SUCCESS;
}

static VOID ToprguPoll(TOPRGU_CONTEXT *c, BOOLEAN backup)
{
    ULONG i;
    BOOLEAN four, feed;
    ULONGLONG elapsed;
    if (InterlockedCompareExchange(&c->busy,1,0) != 0) return;
    if (InterlockedCompareExchange(&c->running,1,1) && c->wdt && c->uart) {
        if (backup && ++c->backupCalls <= 3)
            Trace(c,"A04RGU backup DPC active\n");
        ++c->ticks;
        four=KeQueryActiveProcessorCount(NULL) >= 4;
        elapsed=KeQueryInterruptTime()-c->started;
        feed=mt8167_watchdog_thermal_may_feed(
#if A04_TOPRGU_THERMAL_STOP
                 InterlockedCompareExchange(&c->thermalHalt,0,0),
#else
                 0,
#endif
                 elapsed,A04_TOPRGU_DEADLINE_SECONDS);
        if (feed)
            WRITE_REGISTER_ULONG(c->wdt + 2,0x1971u);
        if (four && !feed && !c->deadlineReported) {
            c->deadlineReported=TRUE;
            Trace(c,"A04RGU bounded watchdog deadline reached\n");
        }
        if (four && feed && (c->ticks <= 3 || c->ticks % 500u == 0))
            Trace(c,"A04RGU timer kick\n");
        if (four && feed && (c->ticks==10 || c->ticks==25 || c->ticks==50 ||
                             c->ticks==100 || c->ticks==250 || c->ticks==500))
            TraceTimer(c,c->ticks,elapsed);
        /* CPU1 owns UART until all four OS processors are online. */
        if (four) {
            InterlockedExchange(&c->ownedFour,1);
            for (i=0; i<16; ++i) {
                if ((READ_REGISTER_ULONG(c->uart + 3) & 0x80u) ||
                    !(READ_REGISTER_ULONG(c->uart + 5) & 1u)) break;
                if (mt8167_reset_consume(&c->parser,(UCHAR)READ_REGISTER_ULONG(c->uart))) {
                    WRITE_REGISTER_ULONG(c->wdt + 5,0x1209u);
                    break;
                }
            }
        }
    }
    InterlockedExchange(&c->busy,0);
}

VOID ToprguTimer(WDFTIMER timer)
{
    ToprguPoll(ToprguGetContext((WDFDEVICE)WdfTimerGetParentObject(timer)),FALSE);
}

VOID ToprguBackupDpc(PKDPC dpc, PVOID context, PVOID arg1, PVOID arg2)
{
    UNREFERENCED_PARAMETER(dpc);
    UNREFERENCED_PARAMETER(arg1);
    UNREFERENCED_PARAMETER(arg2);
    ToprguPoll((TOPRGU_CONTEXT *)context,TRUE);
}

#if A04_TOPRGU_THERMAL_STOP
VOID ToprguThermalControl(WDFQUEUE queue, WDFREQUEST request, size_t outputLength, size_t inputLength, ULONG code)
{
    TOPRGU_CONTEXT *c=ToprguGetContext(WdfIoQueueGetDevice(queue));
    PIRP irp=WdfRequestWdmGetIrp(request);
    NTSTATUS status=STATUS_INVALID_DEVICE_REQUEST;
    UNREFERENCED_PARAMETER(outputLength); UNREFERENCED_PARAMETER(inputLength);
    if (irp->RequestorMode != KernelMode) status=STATUS_ACCESS_DENIED;
    else if (code==A04_TOPRGU_THERMAL_QUERY) {
        status=(c->wdt && InterlockedCompareExchange(&c->running,0,0) &&
                InterlockedCompareExchange(&c->ownedFour,0,0) &&
                !InterlockedCompareExchange(&c->thermalHalt,0,0)) ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
    } else if (code==A04_TOPRGU_THERMAL_HALT) {
        InterlockedExchange(&c->thermalHalt,1);
        if (c->wdt && InterlockedCompareExchange(&c->running,0,0)) {
            WRITE_REGISTER_ULONG(c->wdt+5,0x1209u);
            status=STATUS_SUCCESS; /* request sent; reset is not verified */
        } else status=STATUS_DEVICE_NOT_READY;
    }
    WdfRequestComplete(request,status);
}
#endif
NTSTATUS ToprguAdd(WDFDRIVER driver, PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_OBJECT_ATTRIBUTES attr, timerAttr;
    WDF_TIMER_CONFIG timerConfig;
    WDFDEVICE device;
    TOPRGU_CONTEXT *c;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(driver);
#if A04_TOPRGU_THERMAL_STOP
    { DECLARE_CONST_UNICODE_STRING(name,A04_TOPRGU_THERMAL_NAME);
      status=WdfDeviceInitAssignName(init,&name); if (!NT_SUCCESS(status)) return status; }
#endif
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware=ToprguPrepare; pnp.EvtDeviceReleaseHardware=ToprguRelease;
    pnp.EvtDeviceD0Entry=ToprguD0Entry; pnp.EvtDeviceD0Exit=ToprguD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,TOPRGU_CONTEXT);
    status=WdfDeviceCreate(&init,&attr,&device);
    if (!NT_SUCCESS(status)) return status;
    c=ToprguGetContext(device);
#if A04_TOPRGU_THERMAL_STOP
    { WDF_IO_QUEUE_CONFIG q;
      WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchParallel);
      q.PowerManaged=WdfTrue; q.EvtIoInternalDeviceControl=ToprguThermalControl;
      status=WdfIoQueueCreate(device,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);
      if (!NT_SUCCESS(status)) return status; }
#endif
    KeInitializeTimer(&c->backupTimer);
    KeInitializeDpc(&c->backupDpc,ToprguBackupDpc,c);
    WDF_TIMER_CONFIG_INIT_PERIODIC(&timerConfig,ToprguTimer,20);
    timerConfig.AutomaticSerialization=FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttr); timerAttr.ParentObject=device;
    timerAttr.ExecutionLevel=WdfExecutionLevelDispatch;
    return WdfTimerCreate(&timerConfig,&timerAttr,&c->timer);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config,ToprguAdd);
    return WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
