/* SPDX-License-Identifier: MIT
 * Default-off LC709203F battery function driver. I2C2 must have one owner;
 * this first version never changes clocks, pinmux, charger, or gauge settings.
 */
#include <ntddk.h>
#include <wdf.h>
#include <batclass.h>
#include "battery_class.h"
#include "mt8167_i2c.h"
#include "../bq24160/charger.h"
#include "../bq24160/mt8167_i2c.h"
#ifndef A04_BQ24160_EXPERIMENTAL
#define A04_BQ24160_EXPERIMENTAL 0
#endif
#ifndef A04_LC709203F_TRACE
#define A04_LC709203F_TRACE 0
#endif

#define I2C2_BASE 0x1100b000u
#define I2C2_SIZE 0x1000u

typedef struct {
    WDFDEVICE device;
    WDFTIMER timer;
    WDFWAITLOCK class_lock;
    WDFWAITLOCK bus_lock;
#if A04_BQ24160_EXPERIMENTAL
    struct a04_bq_owner charger;
    struct a04_bq_bus charger_bus;
#endif
    PVOID class_handle;
    volatile UCHAR *i2c;
    struct a04_i2c_io io;
    struct a04_gauge_mt8167 adapter;
    struct a04_gauge_bus bus;
    struct a04_battery_class battery;
    volatile LONG online;
    BOOLEAN faulted;
#if A04_LC709203F_TRACE
    volatile ULONG *uart;
#endif
} GAUGE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(GAUGE_CONTEXT, GaugeContext)
EVT_WDF_DRIVER_DEVICE_ADD GaugeAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE GaugePrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE GaugeRelease;
EVT_WDF_DEVICE_D0_ENTRY GaugeD0Entry;
EVT_WDF_DEVICE_D0_EXIT GaugeD0Exit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT GaugeClassInit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP GaugeClassCleanup;
EVT_WDF_TIMER GaugePoll;
EVT_WDFDEVICE_WDM_IRP_PREPROCESS GaugeIrp;

#if A04_LC709203F_TRACE
static void trace(GAUGE_CONTEXT *c, const char *message)
{
    ULONG n;
    if (!c->uart) return;
    while (*message) {
        for (n = 100000; n; --n)
            if (!(READ_REGISTER_ULONG(c->uart + 3) & 0x80u) &&
                (READ_REGISTER_ULONG(c->uart + 5) & 0x20u)) break;
        if (!n) return;
        WRITE_REGISTER_ULONG(c->uart, (ULONG)(UCHAR)*message++);
    }
}
#else
#define trace(c, message) ((void)0)
#endif

static uint16_t rd(void *context, unsigned offset)
{ GAUGE_CONTEXT *c = context; return READ_REGISTER_USHORT((volatile USHORT *)(c->i2c + offset)); }
static void wr(void *context, unsigned offset, uint16_t value)
{ GAUGE_CONTEXT *c = context; WRITE_REGISTER_USHORT((volatile USHORT *)(c->i2c + offset), value); }
static void delay(void *context, unsigned us)
{ UNREFERENCED_PARAMETER(context); KeStallExecutionProcessor(us); }
static uint64_t now_ns(void *context)
{ UNREFERENCED_PARAMETER(context); return KeQueryInterruptTime() * 100u; }
#if A04_BQ24160_EXPERIMENTAL
/* No Windows firmware handoff protocol exists yet. This gate is intentionally
 * closed even for the experimental build; a compile flag is not proof that
 * firmware stopped accessing I2C2 or that its last pet is recent. */
static int charger_handoff_is_qualified(void) { return 0; }
static uint64_t charger_now(void *context)
{
    struct a04_gauge_mt8167 *a = context;
    return a->monotonic_ns(a->io->context);
}
#endif
static void notify(GAUGE_CONTEXT *c)
{
    WdfWaitLockAcquire(c->class_lock, NULL);
    if (c->class_handle) BatteryClassStatusNotify(c->class_handle);
    WdfWaitLockRelease(c->class_lock);
}
static void poll(GAUGE_CONTEXT *c)
{
    struct a04_gauge_sample sample;
    BOOLEAN changed = FALSE;
#if A04_BQ24160_EXPERIMENTAL
    enum a04_bq_result result;
    uint64_t now;
#endif
    WdfWaitLockAcquire(c->bus_lock, NULL);
    if (!c->online) { WdfWaitLockRelease(c->bus_lock); return; }
#if A04_BQ24160_EXPERIMENTAL
    now = charger_now(&c->adapter);
    /* The first call requires a qualified firmware ownership handoff.
     * This experimental build has no validated handoff record, so it is
     * never eligible for deployment. See README before any target use. */
    result = a04_bq_step(&c->charger, &c->charger_bus,
                          now <= UINT64_MAX - 20000000000ULL ? now + 20000000000ULL : 0);
    if (result != A04_BQ_OK) {
        trace(c, "A04BAT CHARGER STOPPED\n");
        c->faulted = TRUE; InterlockedExchange(&c->online, 0);
        changed = a04_battery_invalidate(&c->battery);
        WdfWaitLockRelease(c->bus_lock);
        if (changed) notify(c);
        return;
    }
    /* Only accepted charger snapshots establish usable external input.
     * A failed or ambiguous sample invalidates the battery status entirely. */
    changed = a04_battery_set_external(&c->battery, TRUE);
#endif
    if (a04_gauge_read_sample(&c->bus, &sample)) {
        trace(c, "A04BAT SAMPLE FAILED\n");
        changed |= a04_battery_invalidate(&c->battery);
#if A04_BQ24160_EXPERIMENTAL
        /* A failed PEC/range check alone leaves the transport healthy.
         * Keep the already-armed charger owner alive; do not publish the gauge.
         * A busy controller, NACK or timeout latches the shared bus fault. */
        if (c->adapter.faulted) {
            c->faulted = TRUE;
            InterlockedExchange(&c->online, 0);
        }
#else
        c->faulted = TRUE;
        InterlockedExchange(&c->online, 0);
#endif
    } else {
        trace(c, "A04BAT SAMPLE VALID\n");
        changed |= a04_battery_publish(&c->battery, &sample);
    }
    WdfWaitLockRelease(c->bus_lock);
    if (changed) notify(c);
}
NTSTATUS GaugePrepare(WDFDEVICE device, WDFCMRESLIST raw, WDFCMRESLIST translated)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    ULONG i, found = 0;
    PHYSICAL_ADDRESS address;
    UNREFERENCED_PARAMETER(raw);
    for (i = 0; i < WdfCmResourceListGetCount(translated); ++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(translated, i);
        if (d && d->Type == CmResourceTypeMemory) {
            if (d->u.Memory.Start.QuadPart != I2C2_BASE || d->u.Memory.Length != I2C2_SIZE || found)
                return STATUS_DEVICE_CONFIGURATION_ERROR;
            address = d->u.Memory.Start; ++found;
        }
    }
    if (found != 1) return STATUS_DEVICE_CONFIGURATION_ERROR;
    c->i2c = MmMapIoSpace(address, I2C2_SIZE, MmNonCached);
    if (!c->i2c) return STATUS_INSUFFICIENT_RESOURCES;
#if A04_LC709203F_TRACE
    { PHYSICAL_ADDRESS uart; uart.QuadPart = 0x11005000u;
      c->uart = MmMapIoSpace(uart, 0x1000u, MmNonCached); }
#endif
    trace(c, "A04BAT PREPARE\n");
    return STATUS_SUCCESS;
}
NTSTATUS GaugeRelease(WDFDEVICE device, WDFCMRESLIST translated)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    UNREFERENCED_PARAMETER(translated);
    if (c->i2c) { MmUnmapIoSpace((PVOID)c->i2c, I2C2_SIZE); c->i2c = NULL; }
#if A04_LC709203F_TRACE
    if (c->uart) { MmUnmapIoSpace((PVOID)c->uart, 0x1000u); c->uart = NULL; }
#endif
    return STATUS_SUCCESS;
}
NTSTATUS GaugeD0Entry(WDFDEVICE device, WDF_POWER_DEVICE_STATE previous)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    UNREFERENCED_PARAMETER(previous);
    if (!c->i2c || c->faulted) return STATUS_DEVICE_NOT_READY;
#if A04_BQ24160_EXPERIMENTAL
    if (!charger_handoff_is_qualified()) return STATUS_DEVICE_NOT_READY;
#endif
    trace(c, "A04BAT D0 ENTRY\n");
    c->io.context = c; c->io.read16 = rd; c->io.write16 = wr; c->io.delay_us = delay;
    c->adapter.io = &c->io; c->adapter.monotonic_ns = now_ns; c->adapter.faulted = 0;
    c->bus.context = &c->adapter; c->bus.read_word_pec = a04_gauge_mt8167_read;
    /* Only accept pre-initialized, idle polling-mode controller state. */
    if ((rd(c, 0x24) & 1u) || !rd(c, 0x20) || (rd(c, 0x48) & 1u) ||
        rd(c, 0x40) != 3u || rd(c, 0x60) != 0u) {
        trace(c, "A04BAT I2C NOT READY\n"); return STATUS_DEVICE_NOT_READY;
    }
    trace(c, "A04BAT I2C READY\n");
#if A04_BQ24160_EXPERIMENTAL
    c->charger_bus.context = &c->adapter;
    c->charger_bus.now_ns = charger_now;
    c->charger_bus.read = a04_bq_mt8167_read;
    c->charger_bus.write_watchdog = a04_bq_mt8167_kick;
#endif
    InterlockedExchange(&c->online, 1);
#if A04_BQ24160_EXPERIMENTAL
    /* Arm before any charger transaction. D0Exit synchronously cancels it. */
    WdfTimerStart(c->timer, WDF_REL_TIMEOUT_IN_MS(10000));
#endif
    poll(c);
    if (!c->online) {
#if A04_BQ24160_EXPERIMENTAL
        WdfTimerStop(c->timer, TRUE);
        WdfTimerStop(c->timer, FALSE);
#endif
        return STATUS_DEVICE_HARDWARE_ERROR;
    }
#if !A04_BQ24160_EXPERIMENTAL
    WdfTimerStart(c->timer, WDF_REL_TIMEOUT_IN_MS(30000));
#endif
    return STATUS_SUCCESS;
}
NTSTATUS GaugeD0Exit(WDFDEVICE device, WDF_POWER_DEVICE_STATE target)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    UNREFERENCED_PARAMETER(target);
    InterlockedExchange(&c->online, 0);
    WdfTimerStop(c->timer, TRUE);
    /* The callback may have rearmed immediately before observing offline. */
    WdfTimerStop(c->timer, FALSE);
    WdfWaitLockAcquire(c->bus_lock, NULL);
#if A04_BQ24160_EXPERIMENTAL
    /* Do not pretend the charger was restored or restart after D0 loss. */
    if (c->charger.attempts) c->faulted = TRUE;
#endif
    WdfWaitLockRelease(c->bus_lock);
    if (a04_battery_invalidate(&c->battery)) notify(c);
    return STATUS_SUCCESS;
}
VOID GaugePoll(WDFTIMER timer)
{
    GAUGE_CONTEXT *c = GaugeContext((WDFDEVICE)WdfTimerGetParentObject(timer));
    poll(c);
#if A04_BQ24160_EXPERIMENTAL
    if (c->online) WdfTimerStart(timer, WDF_REL_TIMEOUT_IN_MS(10000));
#else
    if (c->online) WdfTimerStart(timer, WDF_REL_TIMEOUT_IN_MS(30000));
#endif
}
NTSTATUS GaugeClassInit(WDFDEVICE device)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    BATTERY_MINIPORT_INFO_V1_1 info;
    NTSTATUS status;
    RtlZeroMemory(&info, sizeof(info));
    info.MajorVersion = BATTERY_CLASS_MAJOR_VERSION;
    info.MinorVersion = BATTERY_CLASS_MINOR_VERSION_1;
    info.Context = &c->battery;
    info.QueryTag = a04_battery_query_tag;
    info.QueryInformation = a04_battery_query_information;
    info.SetInformation = a04_battery_set_information;
    info.QueryStatus = a04_battery_query_status;
    info.SetStatusNotify = a04_battery_set_notify;
    info.DisableStatusNotify = a04_battery_disable_notify;
    info.Pdo = WdfDeviceWdmGetPhysicalDevice(device);
    info.Fdo = WdfDeviceWdmGetDeviceObject(device);
    WdfWaitLockAcquire(c->class_lock, NULL);
    status = BatteryClassInitializeDevice((PBATTERY_MINIPORT_INFO)&info, &c->class_handle);
    WdfWaitLockRelease(c->class_lock);
    trace(c, NT_SUCCESS(status) ? "A04BAT CLASS STARTED\n" : "A04BAT CLASS FAILED\n");
    return status;
}
VOID GaugeClassCleanup(WDFDEVICE device)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    WdfWaitLockAcquire(c->class_lock, NULL);
    if (c->class_handle) { BatteryClassUnload(c->class_handle); c->class_handle = NULL; }
    WdfWaitLockRelease(c->class_lock);
}
NTSTATUS GaugeIrp(WDFDEVICE device, PIRP irp)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    NTSTATUS status = STATUS_NOT_SUPPORTED;
    WdfWaitLockAcquire(c->class_lock, NULL);
    if (c->class_handle) status = BatteryClassIoctl(c->class_handle, irp);
    WdfWaitLockRelease(c->class_lock);
    if (status == STATUS_NOT_SUPPORTED) {
        IoSkipCurrentIrpStackLocation(irp);
        status = WdfDeviceWdmDispatchPreprocessedIrp(device, irp);
    }
    return status;
}
NTSTATUS GaugeAdd(WDFDRIVER driver, PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_OBJECT_ATTRIBUTES attributes, child;
    WDF_TIMER_CONFIG timer;
    WDFDEVICE device;
    GAUGE_CONTEXT *c;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(driver);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = GaugePrepare; pnp.EvtDeviceReleaseHardware = GaugeRelease;
    pnp.EvtDeviceD0Entry = GaugeD0Entry; pnp.EvtDeviceD0Exit = GaugeD0Exit;
    pnp.EvtDeviceSelfManagedIoInit = GaugeClassInit; pnp.EvtDeviceSelfManagedIoCleanup = GaugeClassCleanup;
    WdfDeviceInitSetPnpPowerEventCallbacks(init, &pnp);
    status = WdfDeviceInitAssignWdmIrpPreprocessCallback(init, GaugeIrp, IRP_MJ_DEVICE_CONTROL, NULL, 0);
    if (!NT_SUCCESS(status)) return status;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, GAUGE_CONTEXT);
    attributes.ExecutionLevel = WdfExecutionLevelPassive;
    status = WdfDeviceCreate(&init, &attributes, &device);
    if (!NT_SUCCESS(status)) return status;
    c = GaugeContext(device); c->device = device;
    WDF_OBJECT_ATTRIBUTES_INIT(&child); child.ParentObject = device;
    status = WdfWaitLockCreate(&child, &c->class_lock);
    if (!NT_SUCCESS(status)) return status;
    status = WdfWaitLockCreate(&child, &c->bus_lock);
    if (!NT_SUCCESS(status)) return status;
    status = WdfWaitLockCreate(&child, &c->battery.lock);
    if (!NT_SUCCESS(status)) return status;
    /* KMDF passive-level timers cannot be periodic. */
    WDF_TIMER_CONFIG_INIT(&timer, GaugePoll);
    /* D0Exit synchronously stops this timer; do not serialize it under a
     * device callback lock that D0Exit may already hold. */
    timer.AutomaticSerialization = FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&child); child.ParentObject = device; child.ExecutionLevel = WdfExecutionLevelPassive;
    return WdfTimerCreate(&timer, &child, &c->timer);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, GaugeAdd);
    return WdfDriverCreate(driver, registry, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}
