/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 TOPRGU handoff prototype. Hardware ownership and sleep are not yet
 * validated; do not deploy as the only recovery path without a rollback. */
#include <ntddk.h>
#include <wdf.h>
#include "reset_command.h"

typedef struct {
    volatile ULONG *wdt, *uart;
    WDFTIMER timer;
    volatile LONG running, busy;
    ULONG ticks;
    struct mt8167_reset_parser parser;
} TOPRGU_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(TOPRGU_CONTEXT, ToprguGetContext)
EVT_WDF_DRIVER_DEVICE_ADD ToprguAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE ToprguPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE ToprguRelease;
EVT_WDF_DEVICE_D0_ENTRY ToprguD0Entry;
EVT_WDF_DEVICE_D0_EXIT ToprguD0Exit;
EVT_WDF_TIMER ToprguTimer;

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
    if (!c->wdt || !(READ_REGISTER_ULONG(c->wdt) & 1u)) return STATUS_DEVICE_NOT_READY;
    WRITE_REGISTER_ULONG(c->wdt + 2,0x1971u);
    c->parser.matched=0; c->ticks=0;
    InterlockedExchange(&c->running,1);
    Trace(c, KeQueryActiveProcessorCount(NULL) >= 4 ?
          "A04RGU D0 active CPUs>=4\n" : "A04RGU D0 active CPUs<4\n");
    WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(20));
    return STATUS_SUCCESS;
}

NTSTATUS ToprguD0Exit(WDFDEVICE device, WDF_POWER_DEVICE_STATE target)
{
    TOPRGU_CONTEXT *c=ToprguGetContext(device);
    UNREFERENCED_PARAMETER(target);
    InterlockedExchange(&c->running,0);
    WdfTimerStop(c->timer,TRUE);
    /* Sleep handling is intentionally not implemented in this prototype. */
    return STATUS_SUCCESS;
}

VOID ToprguTimer(WDFTIMER timer)
{
    TOPRGU_CONTEXT *c=ToprguGetContext((WDFDEVICE)WdfTimerGetParentObject(timer));
    ULONG i;
    if (InterlockedCompareExchange(&c->busy,1,0) != 0) return;
    if (InterlockedCompareExchange(&c->running,1,1) && c->wdt && c->uart) {
        if ((++c->ticks % 50u) == 0) WRITE_REGISTER_ULONG(c->wdt + 2,0x1971u);
        /* CPU1 owns UART until all four OS processors are online. */
        if (KeQueryActiveProcessorCount(NULL) >= 4) {
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

NTSTATUS ToprguAdd(WDFDRIVER driver, PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_OBJECT_ATTRIBUTES attr, timerAttr;
    WDF_TIMER_CONFIG timerConfig;
    WDFDEVICE device;
    TOPRGU_CONTEXT *c;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(driver);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware=ToprguPrepare; pnp.EvtDeviceReleaseHardware=ToprguRelease;
    pnp.EvtDeviceD0Entry=ToprguD0Entry; pnp.EvtDeviceD0Exit=ToprguD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,TOPRGU_CONTEXT);
    status=WdfDeviceCreate(&init,&attr,&device);
    if (!NT_SUCCESS(status)) return status;
    c=ToprguGetContext(device);
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
