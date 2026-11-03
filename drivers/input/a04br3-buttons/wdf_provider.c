/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental polling provider. PnP assigns the only MMIO ranges used here.
 * It does not implement interrupts, power-button wake or production recovery.
 * Delivery remains disabled unless a separate platform integration establishes
 * a quiescent notification endpoint and calls the synchronization API. */
#include <ntddk.h>
#include <wdf.h>
#include "button_hardware.h"

#define A04_KEYPAD_BASE 0x10002000u
#define A04_PWRAP_BASE  0x1000f000u
#define A04_RANGE_SIZE  0x1000u

typedef struct {
    WDFTIMER timer;
    volatile UCHAR *keypad;
    volatile UCHAR *pwrap;
    struct a04_button_hardware hardware;
} A04_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(A04_CONTEXT, A04GetContext)
EVT_WDF_DRIVER_DEVICE_ADD A04DeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE A04Prepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE A04Release;
EVT_WDF_DEVICE_D0_ENTRY A04D0Entry;
EVT_WDF_DEVICE_D0_EXIT A04D0Exit;
EVT_WDF_TIMER A04Timer;

static uint32_t PwrapRead(void *context, unsigned offset)
{
    A04_CONTEXT *c = context;
    return READ_REGISTER_ULONG((volatile ULONG *)(c->pwrap + offset));
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
static int KeypadRead(void *context, unsigned offset, uint16_t *value)
{
    A04_CONTEXT *c = context;
    if (!c->keypad || offset != 4) return -1;
    *value = READ_REGISTER_USHORT((volatile USHORT *)(c->keypad + offset));
    return 0;
}
static int NoEndpoint(void *context, uint8_t button)
{
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(button);
    /* No attempted write: there is no endpoint or synchronization yet. */
    return 1;
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
    if (!haveKeypad || !havePwrap) return STATUS_DEVICE_CONFIGURATION_ERROR;
    c->keypad = MmMapIoSpace(keypad, A04_RANGE_SIZE, MmNonCached);
    if (!c->keypad) return STATUS_INSUFFICIENT_RESOURCES;
    c->pwrap = MmMapIoSpace(pwrap, A04_RANGE_SIZE, MmNonCached);
    if (!c->pwrap) {
        MmUnmapIoSpace((PVOID)c->keypad, A04_RANGE_SIZE); c->keypad = NULL;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    c->hardware.context = c;
    c->hardware.keypad_read16 = KeypadRead;
    c->hardware.notify = NoEndpoint;
    c->hardware.pmic.context = c;
    c->hardware.pmic.read32 = PwrapRead;
    c->hardware.pmic.write32 = PwrapWrite;
    c->hardware.pmic.delay = PwrapDelay;
    c->hardware.pmic.attempts = 1000;
    c->hardware.pmic.poisoned = 0;
    a04_button_hardware_init(&c->hardware);
    return STATUS_SUCCESS;
}

NTSTATUS A04Release(WDFDEVICE device, WDFCMRESLIST translated)
{
    A04_CONTEXT *c = A04GetContext(device);
    UNREFERENCED_PARAMETER(translated);
    WdfTimerStop(c->timer, TRUE);
    a04_button_device_disconnect(&c->hardware.device);
    if (c->pwrap) { MmUnmapIoSpace((PVOID)c->pwrap, A04_RANGE_SIZE); c->pwrap = NULL; }
    if (c->keypad) { MmUnmapIoSpace((PVOID)c->keypad, A04_RANGE_SIZE); c->keypad = NULL; }
    return STATUS_SUCCESS;
}
NTSTATUS A04D0Entry(WDFDEVICE device, WDF_POWER_DEVICE_STATE previous)
{
    A04_CONTEXT *c = A04GetContext(device);
    UNREFERENCED_PARAMETER(previous);
    /* Disabled until endpoint synchronization and safe ownership are solved. */
    UNREFERENCED_PARAMETER(c);
    return STATUS_SUCCESS;
}
NTSTATUS A04D0Exit(WDFDEVICE device, WDF_POWER_DEVICE_STATE target)
{
    A04_CONTEXT *c = A04GetContext(device);
    UNREFERENCED_PARAMETER(target);
    WdfTimerStop(c->timer, TRUE);
    a04_button_device_disconnect(&c->hardware.device);
    return STATUS_SUCCESS;
}
void A04Timer(WDFTIMER timer)
{
    WDFDEVICE device = (WDFDEVICE)WdfTimerGetParentObject(timer);
    A04_CONTEXT *c = A04GetContext(device);
    if (c->keypad && c->pwrap) (void)a04_button_device_poll(&c->hardware.device);
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
    WDF_TIMER_CONFIG_INIT_PERIODIC(&timerConfig, A04Timer, 30);
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
