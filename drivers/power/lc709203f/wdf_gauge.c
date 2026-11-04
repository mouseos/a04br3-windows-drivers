/* SPDX-License-Identifier: MIT
 * Default-off LC709203F battery function driver. I2C2 must have one owner;
 * this first version never changes clocks, pinmux, charger, or gauge settings.
 */
#include <ntddk.h>
#include <wdf.h>
#include <batclass.h>
#include "battery_class.h"
#include "mt8167_i2c.h"

#define I2C2_BASE 0x1100b000u
#define I2C2_SIZE 0x1000u

typedef struct {
    WDFDEVICE device;
    WDFTIMER timer;
    WDFWAITLOCK class_lock;
    PVOID class_handle;
    volatile UCHAR *i2c;
    struct a04_i2c_io io;
    struct a04_gauge_mt8167 adapter;
    struct a04_gauge_bus bus;
    struct a04_battery_class battery;
    BOOLEAN online;
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
EVT_WDF_DEVICE_WDM_IRP_PREPROCESS GaugeIrp;

static uint16_t rd(void *context, unsigned offset)
{ GAUGE_CONTEXT *c = context; return READ_REGISTER_USHORT((volatile USHORT *)(c->i2c + offset)); }
static void wr(void *context, unsigned offset, uint16_t value)
{ GAUGE_CONTEXT *c = context; WRITE_REGISTER_USHORT((volatile USHORT *)(c->i2c + offset), value); }
static void delay(void *context, unsigned us)
{ UNREFERENCED_PARAMETER(context); KeStallExecutionProcessor(us); }
static uint64_t now_ns(void *context)
{ UNREFERENCED_PARAMETER(context); return KeQueryInterruptTime() * 100u; }
static void notify(GAUGE_CONTEXT *c)
{
    WdfWaitLockAcquire(c->class_lock, NULL);
    if (c->class_handle) BatteryClassStatusNotify(c->class_handle);
    WdfWaitLockRelease(c->class_lock);
}
static void poll(GAUGE_CONTEXT *c)
{
    struct a04_gauge_sample sample;
    BOOLEAN changed;
    if (!c->online) return;
    if (a04_gauge_read_sample(&c->bus, &sample)) {
        changed = a04_battery_invalidate(&c->battery);
        c->online = FALSE; /* No reset/retry of shared I2C2 after failure. */
    } else changed = a04_battery_publish(&c->battery, &sample);
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
    return STATUS_SUCCESS;
}
NTSTATUS GaugeRelease(WDFDEVICE device, WDFCMRESLIST translated)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    UNREFERENCED_PARAMETER(translated);
    if (c->i2c) { MmUnmapIoSpace((PVOID)c->i2c, I2C2_SIZE); c->i2c = NULL; }
    return STATUS_SUCCESS;
}
NTSTATUS GaugeD0Entry(WDFDEVICE device, WDF_POWER_DEVICE_STATE previous)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    UNREFERENCED_PARAMETER(previous);
    if (!c->i2c) return STATUS_DEVICE_NOT_READY;
    c->io.context = c; c->io.read16 = rd; c->io.write16 = wr; c->io.delay_us = delay;
    c->adapter.io = &c->io; c->adapter.monotonic_ns = now_ns; c->adapter.faulted = 0;
    c->bus.context = &c->adapter; c->bus.read_word_pec = a04_gauge_mt8167_read;
    /* Only accept pre-initialized, idle polling-mode controller state. */
    if ((rd(c, 0x24) & 1u) || !rd(c, 0x20) || (rd(c, 0x48) & 1u) ||
        rd(c, 0x40) != 3u || rd(c, 0x60) != 0u) return STATUS_DEVICE_NOT_READY;
    c->online = TRUE;
    poll(c);
    if (!c->online) return STATUS_DEVICE_HARDWARE_ERROR;
    WdfTimerStart(c->timer, WDF_REL_TIMEOUT_IN_MS(30000));
    return STATUS_SUCCESS;
}
NTSTATUS GaugeD0Exit(WDFDEVICE device, WDF_POWER_DEVICE_STATE target)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    UNREFERENCED_PARAMETER(target);
    WdfTimerStop(c->timer, TRUE);
    c->online = FALSE;
    if (a04_battery_invalidate(&c->battery)) notify(c);
    return STATUS_SUCCESS;
}
VOID GaugePoll(WDFTIMER timer)
{
    GAUGE_CONTEXT *c = GaugeContext((WDFDEVICE)WdfTimerGetParentObject(timer));
    poll(c);
    if (c->online) WdfTimerStart(timer, WDF_REL_TIMEOUT_IN_MS(30000));
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
    status = WdfWaitLockCreate(&child, &c->battery.lock);
    if (!NT_SUCCESS(status)) return status;
    WDF_TIMER_CONFIG_INIT(&timer, GaugePoll);
    WDF_OBJECT_ATTRIBUTES_INIT(&child); child.ParentObject = device; child.ExecutionLevel = WdfExecutionLevelPassive;
    return WdfTimerCreate(&timer, &child, &c->timer);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, GaugeAdd);
    return WdfDriverCreate(driver, registry, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}
