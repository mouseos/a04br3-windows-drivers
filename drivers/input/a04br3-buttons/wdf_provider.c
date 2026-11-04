/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental polling provider. PnP assigns the only MMIO ranges used here.
 * It does not implement interrupts, power-button wake or production recovery.
 * Delivery is disabled by default. A controlled, first-boot-only experiment
 * can opt into an explicit assumption that the endpoint queue is pristine;
 * this is not a general way to establish queue quiescence. */
#include <ntddk.h>
#include <wdf.h>
#include "button_hardware.h"
#ifndef A04_BUTTON_SHARED_PARENT
#define A04_BUTTON_SHARED_PARENT 0
#endif
#if A04_BUTTON_SHARED_PARENT
#include "../../bus/mt8167-shared/client_interface.h"
#endif

#define A04_KEYPAD_BASE 0x10002000u
#define A04_PWRAP_BASE  0x1000f000u
#define A04_RANGE_SIZE  0x1000u
#ifndef A04_ASSUME_PRISTINE_BUTTON_QUEUE
#define A04_ASSUME_PRISTINE_BUTTON_QUEUE 0
#endif
#ifndef A04_BUTTON_TRACE
#define A04_BUTTON_TRACE 0
#endif

static const GUID ButtonInterface =
    {0x30ebfbf8,0xdf5f,0x4d4d,{0x9f,0xc5,0xa2,0x6c,0x7f,0xd1,0xdf,0x4a}};

typedef struct {
    WDFTIMER timer;
#if A04_BUTTON_SHARED_PARENT
    A04_SHARED_INTERFACE shared;
#endif
    volatile UCHAR *keypad;
    volatile UCHAR *pwrap;
    struct a04_button_hardware hardware;
    HANDLE endpoint;
    volatile LONG timerBusy;
    volatile LONG timerEnabled;
    BOOLEAN attemptedSync;
    BOOLEAN permanentlyDisabled;
    BOOLEAN awaitingReleased;
    UCHAR baseline[5];
#if A04_BUTTON_TRACE
    volatile ULONG *uart;
    ULONG ticks;
    USHORT lastKeypad;
    USHORT lastPmic;
#endif
} A04_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(A04_CONTEXT, A04GetContext)
EVT_WDF_DRIVER_DEVICE_ADD A04DeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE A04Prepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE A04Release;
EVT_WDF_DEVICE_D0_ENTRY A04D0Entry;
EVT_WDF_DEVICE_D0_EXIT A04D0Exit;
EVT_WDF_TIMER A04Timer;

#if A04_BUTTON_TRACE
/* Temporary diagnostics only: shares the board UART with the monitor. */
static void Trace(A04_CONTEXT *c, const char *s)
{
    ULONG budget;
    if (!c->uart) return;
    while (*s) {
        for (budget=100000; budget; --budget)
            if (!(READ_REGISTER_ULONG(c->uart + 3) & 0x80u) &&
                (READ_REGISTER_ULONG(c->uart + 5) & 0x20u)) break;
        if (!budget) return;
        WRITE_REGISTER_ULONG(c->uart, (ULONG)(UCHAR)*s++);
    }
}
static void TraceHex(A04_CONTEXT *c, ULONG value)
{
    static const char digits[]="0123456789ABCDEF";
    char s[9]; unsigned i;
    for (i=0; i<8; ++i) s[i]=digits[(value >> (28-4*i)) & 15u];
    s[8]=0; Trace(c,s);
}
#endif

static uint32_t PwrapRead(void *context, unsigned offset)
{
    A04_CONTEXT *c = context;
    ULONG value = READ_REGISTER_ULONG((volatile ULONG *)(c->pwrap + offset));
#if A04_BUTTON_TRACE
    if (offset == 0xa4u && ((value >> 16) & 7u) == 6u) c->lastPmic = (USHORT)value;
#endif
    return value;
}
static void PwrapWrite(void *context, unsigned offset, uint32_t value)
{
    A04_CONTEXT *c = context;
    WRITE_REGISTER_ULONG((volatile ULONG *)(c->pwrap + offset), value);
}
static void PwrapDelay(void *context)
{
    UNREFERENCED_PARAMETER(context);
    KeStallExecutionProcessor(10);
}
#if A04_BUTTON_SHARED_PARENT
static int SharedPmicRead(void *p, uint16_t *v)
{ A04_CONTEXT *c=p; return c->shared.ButtonPmicRead(c->shared.Context,v); }
#endif
static int KeypadRead(void *context, unsigned offset, uint16_t *value)
{
    A04_CONTEXT *c = context;
    if (!c->keypad || offset != 4) return -1;
    *value = READ_REGISTER_USHORT((volatile USHORT *)(c->keypad + offset));
#if A04_BUTTON_TRACE
    c->lastKeypad = *value;
#endif
    return 0;
}
static int Notify(void *context, uint8_t button)
{
    A04_CONTEXT *c = context;
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    if (!c->endpoint) return 1;
    io.Status = STATUS_UNSUCCESSFUL; io.Information = 0;
    status = ZwWriteFile(c->endpoint, NULL, NULL, NULL, &io, &button, 1, NULL, NULL);
#if A04_BUTTON_TRACE
    Trace(c,"A04BTP notify button="); TraceHex(c,button);
    Trace(c," status="); TraceHex(c,(ULONG)status);
    Trace(c," bytes="); TraceHex(c,(ULONG)io.Information); Trace(c,"\n");
#endif
    if (NT_SUCCESS(status) && io.Information == 1) return 0;
    /* A failed or short write may already have toggled state. */
    c->permanentlyDisabled = TRUE;
    return -1;
}

static void TryEndpoint(A04_CONTEXT *c)
{
    PWSTR links = NULL;
    UNICODE_STRING path;
    OBJECT_ATTRIBUTES attributes;
    IO_STATUS_BLOCK io;
    UCHAR state[5] = {0};
    NTSTATUS status;
    unsigned i;
    if (c->endpoint || c->permanentlyDisabled || c->attemptedSync) return;
    status = IoGetDeviceInterfaces(&ButtonInterface, NULL, 0, &links);
    if (!NT_SUCCESS(status) || !links) { if (links) ExFreePool(links); return; }
    if (!links[0]) { ExFreePool(links); return; }
    RtlInitUnicodeString(&path, links);
    InitializeObjectAttributes(&attributes, &path, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                               NULL, NULL);
    status = ZwCreateFile(&c->endpoint, FILE_READ_DATA | FILE_WRITE_DATA | SYNCHRONIZE,
                          &attributes, &io, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                          FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    ExFreePool(links);
#if A04_BUTTON_TRACE
    Trace(c,"A04BTP endpoint open status="); TraceHex(c,(ULONG)status); Trace(c,"\n");
#endif
    if (!NT_SUCCESS(status)) { c->endpoint = NULL; return; }
    c->attemptedSync = TRUE;
    io.Status = STATUS_UNSUCCESSFUL; io.Information = 0;
    status = ZwReadFile(c->endpoint, NULL, NULL, NULL, &io, state, sizeof(state), NULL, NULL);
#if A04_BUTTON_TRACE
    Trace(c,"A04BTP endpoint read status="); TraceHex(c,(ULONG)status);
    Trace(c," bytes="); TraceHex(c,(ULONG)io.Information); Trace(c," state=");
    for (i=0; i<sizeof(state); ++i) { TraceHex(c,state[i]); Trace(c," "); }
    Trace(c,"\n");
#endif
    if (!NT_SUCCESS(status) || io.Information != sizeof(state)) goto reject;
    /* Only the controlled test boot permits the explicitly unproven queue
     * assumption. Nonzero/special state or later D0 entries fail closed. */
    for (i = 0; i < sizeof(state); ++i) if (state[i] != 0) goto reject;
    if (A04_ASSUME_PRISTINE_BUTTON_QUEUE) {
        RtlCopyMemory(c->baseline, state, sizeof(state));
        /* Sample both physical sources while unsynchronized. Never infer
         * release from a failed source or emit ghost presses at startup. */
        c->awaitingReleased = TRUE;
        return;
    }
reject:
    ZwClose(c->endpoint); c->endpoint = NULL; c->permanentlyDisabled = TRUE;
}

NTSTATUS A04Prepare(WDFDEVICE device, WDFCMRESLIST raw, WDFCMRESLIST translated)
{
    A04_CONTEXT *c = A04GetContext(device);
    ULONG i, count = WdfCmResourceListGetCount(translated);
    PHYSICAL_ADDRESS keypad, pwrap;
    BOOLEAN haveKeypad = FALSE, havePwrap = FALSE;
    UNREFERENCED_PARAMETER(raw);
    keypad.QuadPart = pwrap.QuadPart = 0;
    for (i = 0; i < count; ++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(translated, i);
        if (!d || d->Type != CmResourceTypeMemory) continue;
        if (d->u.Memory.Length != A04_RANGE_SIZE) return STATUS_DEVICE_CONFIGURATION_ERROR;
        if (d->u.Memory.Start.QuadPart == A04_KEYPAD_BASE && !haveKeypad) {
            keypad = d->u.Memory.Start; haveKeypad = TRUE;
        } else if (d->u.Memory.Start.QuadPart == A04_PWRAP_BASE && !havePwrap) {
            pwrap = d->u.Memory.Start; havePwrap = TRUE;
        } else return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
#if A04_BUTTON_SHARED_PARENT
    if (!haveKeypad || havePwrap) return STATUS_DEVICE_CONFIGURATION_ERROR;
    {
        NTSTATUS status = WdfFdoQueryForInterface(device, &A04SharedInterfaceGuid,
            (PINTERFACE)&c->shared, sizeof(c->shared), A04_SHARED_INTERFACE_VERSION, NULL);
        if (!NT_SUCCESS(status)) return status;
        if (!c->shared.InterfaceDereference || !c->shared.ButtonPmicRead) {
            if (c->shared.InterfaceDereference) c->shared.InterfaceDereference(c->shared.Context);
            RtlZeroMemory(&c->shared, sizeof(c->shared));
            return STATUS_NOINTERFACE;
        }
    }
#else
    if (!haveKeypad || !havePwrap) return STATUS_DEVICE_CONFIGURATION_ERROR;
#endif
    c->keypad = MmMapIoSpace(keypad, A04_RANGE_SIZE, MmNonCached);
    if (!c->keypad) {
#if A04_BUTTON_SHARED_PARENT
        c->shared.InterfaceDereference(c->shared.Context); RtlZeroMemory(&c->shared,sizeof(c->shared));
#endif
        return STATUS_INSUFFICIENT_RESOURCES;
    }
#if !A04_BUTTON_SHARED_PARENT
    c->pwrap = MmMapIoSpace(pwrap, A04_RANGE_SIZE, MmNonCached);
    if (!c->pwrap) {
        MmUnmapIoSpace((PVOID)c->keypad, A04_RANGE_SIZE); c->keypad = NULL;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
#endif
#if A04_BUTTON_SHARED_PARENT
    c->hardware.shared_pmic=1;
    c->hardware.shared_pmic_context=c;
    c->hardware.shared_pmic_read=SharedPmicRead;
#endif
    c->hardware.context = c;
    c->hardware.keypad_read16 = KeypadRead;
    c->hardware.notify = Notify;
    c->hardware.pmic.context = c;
    c->hardware.pmic.read32 = PwrapRead;
    c->hardware.pmic.write32 = PwrapWrite;
    c->hardware.pmic.delay = PwrapDelay;
    c->hardware.pmic.attempts = 1000;
    a04_button_hardware_init(&c->hardware);
#if A04_BUTTON_TRACE
    { PHYSICAL_ADDRESS uart; uart.QuadPart=0x11005000u;
      c->uart=(volatile ULONG *)MmMapIoSpace(uart,A04_RANGE_SIZE,MmNonCached);
      Trace(c,"A04BTP prepare complete\n"); }
#endif
    return STATUS_SUCCESS;
}

NTSTATUS A04Release(WDFDEVICE device, WDFCMRESLIST translated)
{
    A04_CONTEXT *c = A04GetContext(device);
    UNREFERENCED_PARAMETER(translated);
    InterlockedExchange(&c->timerEnabled, 0);
    WdfTimerStop(c->timer, TRUE);
    /* A callback that was already running may have rearmed before it saw
     * timerEnabled change; cancel that last one-shot after it has exited. */
    WdfTimerStop(c->timer, TRUE);
    a04_button_device_disconnect(&c->hardware.device);
    if (c->endpoint) { ZwClose(c->endpoint); c->endpoint = NULL; }
#if A04_BUTTON_TRACE
    Trace(c,"A04BTP release\n");
    if (c->uart) { MmUnmapIoSpace((PVOID)c->uart,A04_RANGE_SIZE); c->uart=NULL; }
#endif
    if (c->pwrap) { MmUnmapIoSpace((PVOID)c->pwrap, A04_RANGE_SIZE); c->pwrap = NULL; }
    if (c->keypad) { MmUnmapIoSpace((PVOID)c->keypad, A04_RANGE_SIZE); c->keypad = NULL; }
#if A04_BUTTON_SHARED_PARENT
    if (c->shared.InterfaceDereference) { c->shared.InterfaceDereference(c->shared.Context); RtlZeroMemory(&c->shared,sizeof(c->shared)); }
#endif
    return STATUS_SUCCESS;
}
NTSTATUS A04D0Entry(WDFDEVICE device, WDF_POWER_DEVICE_STATE previous)
{
    A04_CONTEXT *c = A04GetContext(device);
    UNREFERENCED_PARAMETER(previous);
#if A04_BUTTON_TRACE
    Trace(c,"A04BTP D0 entry\n");
#endif
    if (!c->attemptedSync && !c->permanentlyDisabled) {
        InterlockedExchange(&c->timerEnabled, 1);
        WdfTimerStart(c->timer, WDF_REL_TIMEOUT_IN_MS(30));
    }
    return STATUS_SUCCESS;
}
NTSTATUS A04D0Exit(WDFDEVICE device, WDF_POWER_DEVICE_STATE target)
{
    A04_CONTEXT *c = A04GetContext(device);
    UNREFERENCED_PARAMETER(target);
#if A04_BUTTON_TRACE
    Trace(c,"A04BTP D0 exit\n");
#endif
    InterlockedExchange(&c->timerEnabled, 0);
    WdfTimerStop(c->timer, TRUE);
    WdfTimerStop(c->timer, TRUE);
    a04_button_device_disconnect(&c->hardware.device);
    if (c->endpoint) { ZwClose(c->endpoint); c->endpoint = NULL; }
    /* Do not assume the endpoint remains synchronized across power changes. */
    if (c->attemptedSync) c->permanentlyDisabled = TRUE;
    return STATUS_SUCCESS;
}
void A04Timer(WDFTIMER timer)
{
    WDFDEVICE device = (WDFDEVICE)WdfTimerGetParentObject(timer);
    A04_CONTEXT *c = A04GetContext(device);
    if (InterlockedCompareExchange(&c->timerBusy, 1, 0) != 0) return;
    if (c->keypad && (c->pwrap
#if A04_BUTTON_SHARED_PARENT
        || c->shared.ButtonPmicRead
#endif
        )) {
        TryEndpoint(c);
        if (c->endpoint && !c->permanentlyDisabled) {
            (void)a04_button_device_poll(&c->hardware.device);
            if (c->awaitingReleased &&
                (c->hardware.device.pipeline.known & 0x0f) == 0x0f &&
                (c->hardware.device.pipeline.stable & 0x0f) == 0) {
                c->awaitingReleased = FALSE;
                if (a04_button_device_sync_report(&c->hardware.device, c->baseline,
                                                   sizeof(c->baseline), TRUE) != 0)
                    c->permanentlyDisabled = TRUE;
#if A04_BUTTON_TRACE
                Trace(c,"A04BTP baseline synchronized\n");
#endif
            }
        }
    }
#if A04_BUTTON_TRACE
    if ((++c->ticks % 33u) == 1u) {
        Trace(c,"A04BTP sample keypad="); TraceHex(c,c->lastKeypad);
        Trace(c," pmic="); TraceHex(c,c->lastPmic);
        Trace(c," known="); TraceHex(c,c->hardware.device.pipeline.known);
        Trace(c," stable="); TraceHex(c,c->hardware.device.pipeline.stable);
        Trace(c," sync="); TraceHex(c,c->hardware.device.pipeline.synchronized);
        Trace(c," poisoned="); TraceHex(c,(ULONG)c->hardware.pmic.poisoned); Trace(c,"\n");
    }
#endif
    if (InterlockedCompareExchange(&c->timerEnabled, 1, 1) && !c->permanentlyDisabled)
        WdfTimerStart(timer, WDF_REL_TIMEOUT_IN_MS(30));
    InterlockedExchange(&c->timerBusy, 0);
}

NTSTATUS A04DeviceAdd(WDFDRIVER driver, PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_OBJECT_ATTRIBUTES attr, timerAttr;
    WDF_TIMER_CONFIG timerConfig;
    WDFDEVICE device;
    A04_CONTEXT *c;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(driver);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = A04Prepare;
    pnp.EvtDeviceReleaseHardware = A04Release;
    pnp.EvtDeviceD0Entry = A04D0Entry;
    pnp.EvtDeviceD0Exit = A04D0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(init, &pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, A04_CONTEXT);
    status = WdfDeviceCreate(&init, &attr, &device);
    if (!NT_SUCCESS(status)) return status;
    c = A04GetContext(device);
    /* KMDF does not support periodic timers at PASSIVE_LEVEL. */
    WDF_TIMER_CONFIG_INIT(&timerConfig, A04Timer);
    timerConfig.AutomaticSerialization = FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttr);
    timerAttr.ParentObject = device;
    timerAttr.ExecutionLevel = WdfExecutionLevelPassive;
    return WdfTimerCreate(&timerConfig, &timerAttr, &c->timer);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, A04DeviceAdd);
    return WdfDriverCreate(driver, path, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}
