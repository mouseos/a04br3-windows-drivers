/* SPDX-License-Identifier: MIT
 * Battery class callback contract: Microsoft Windows-driver-samples/simbatt.
 */
#include "battery_class.h"
BOOLEAN a04_battery_publish(struct a04_battery_class *c,
                            const struct a04_gauge_sample *sample)
{
    return a04_battery_publish_power(c, sample, 0);
}
BOOLEAN a04_battery_publish_power(struct a04_battery_class *c,
                                  const struct a04_gauge_sample *sample, ULONG power_state)
{
    BOOLEAN changed;
    WdfWaitLockAcquire(c->lock, NULL);
    changed = !c->valid || c->sample.percent != sample->percent ||
              c->sample.voltage_mv != sample->voltage_mv || c->power_state != power_state;
    if (!c->valid) {
        ++c->tag;
        if (c->tag == BATTERY_TAG_INVALID) ++c->tag;
    }
    c->sample = *sample;
    c->power_state = power_state;
    c->valid = TRUE;
    WdfWaitLockRelease(c->lock);
    return changed;
}
BOOLEAN a04_battery_invalidate(struct a04_battery_class *c)
{
    BOOLEAN changed;
    WdfWaitLockAcquire(c->lock, NULL);
    changed = c->valid;
    c->valid = FALSE;
    c->power_state = 0;
    WdfWaitLockRelease(c->lock);
    return changed;
}
NTSTATUS a04_battery_query_tag(PVOID context, PULONG tag)
{
    struct a04_battery_class *c = context;
    NTSTATUS status;
    WdfWaitLockAcquire(c->lock, NULL);
    *tag = c->valid ? c->tag : BATTERY_TAG_INVALID;
    status = c->valid ? STATUS_SUCCESS : STATUS_NO_SUCH_DEVICE;
    WdfWaitLockRelease(c->lock);
    return status;
}
NTSTATUS a04_battery_query_information(PVOID context, ULONG tag,
        BATTERY_QUERY_INFORMATION_LEVEL level, LONG rate, PVOID buffer,
        ULONG length, PULONG returned)
{
    struct a04_battery_class *c = context;
    BATTERY_INFORMATION info;
    const void *source = NULL;
    ULONG bytes = 0;
    NTSTATUS status = STATUS_SUCCESS;
    static const WCHAR name[] = L"LC709203F";
    UNREFERENCED_PARAMETER(rate);
    if (level == BatteryTemperature) {
        ULONG kelvin10 = 0;
        PVOID reader_context = NULL;
        NTSTATUS (*reader)(PVOID, PULONG) = NULL;
        /* Poll acquires bus -> class. Do not invert that lock order. */
        WdfWaitLockAcquire(c->lock, NULL);
        *returned = 0;
        if (!c->valid || tag != c->tag) status = STATUS_NO_SUCH_DEVICE;
        else if (!c->read_temperature) status = STATUS_INVALID_DEVICE_REQUEST;
        else if (!buffer || length < sizeof(kelvin10)) {
            *returned = sizeof(kelvin10); status = STATUS_BUFFER_TOO_SMALL;
        } else { reader = c->read_temperature; reader_context = c->temperature_context; }
        WdfWaitLockRelease(c->lock);
        if (!NT_SUCCESS(status)) return status;
        status = reader(reader_context, &kelvin10);
        if (!NT_SUCCESS(status)) return status;
        WdfWaitLockAcquire(c->lock, NULL);
        /* D0 exit/re-entry and changed battery tags invalidate in-flight reads. */
        if (!c->valid || tag != c->tag) status = STATUS_NO_SUCH_DEVICE;
        else { RtlCopyMemory(buffer, &kelvin10, sizeof(kelvin10)); *returned = sizeof(kelvin10); }
        WdfWaitLockRelease(c->lock);
        return status;
    }
    WdfWaitLockAcquire(c->lock, NULL);
    *returned = 0;
    if (!c->valid || tag != c->tag) status = STATUS_NO_SUCH_DEVICE;
    else if (level == BatteryInformation) {
        RtlZeroMemory(&info, sizeof(info));
        info.Capabilities = BATTERY_SYSTEM_BATTERY | BATTERY_CAPACITY_RELATIVE;
        info.Technology = 1;
        RtlCopyMemory(info.Chemistry, "LION", 4);
        info.DesignedCapacity = BATTERY_UNKNOWN_CAPACITY;
        info.FullChargedCapacity = 100;
        /* The gauge has not established manufacturer alerts or cycle count. */
        source = &info; bytes = sizeof(info);
    } else if (level == BatteryDeviceName) { source = name; bytes = sizeof(name); }
    else status = STATUS_INVALID_DEVICE_REQUEST;
    if (NT_SUCCESS(status)) {
        *returned = bytes;
        if (!buffer || length < bytes) status = STATUS_BUFFER_TOO_SMALL;
        else RtlCopyMemory(buffer, source, bytes);
    }
    WdfWaitLockRelease(c->lock);
    return status;
}
NTSTATUS a04_battery_query_status(PVOID context, ULONG tag, PBATTERY_STATUS status)
{
    struct a04_battery_class *c = context;
    NTSTATUS result = STATUS_SUCCESS;
    WdfWaitLockAcquire(c->lock, NULL);
    if (!c->valid || tag != c->tag) result = STATUS_NO_SUCH_DEVICE;
    else {
        status->PowerState = c->power_state;
        status->Capacity = c->sample.percent;
        status->Voltage = c->sample.voltage_mv;
        status->Rate = BATTERY_UNKNOWN_RATE;
    }
    WdfWaitLockRelease(c->lock);
    return result;
}
NTSTATUS a04_battery_set_information(PVOID context, ULONG tag,
                                     BATTERY_SET_INFORMATION_LEVEL level, PVOID buffer)
{
    UNREFERENCED_PARAMETER(context); UNREFERENCED_PARAMETER(tag);
    UNREFERENCED_PARAMETER(level); UNREFERENCED_PARAMETER(buffer);
    return STATUS_NOT_SUPPORTED;
}
NTSTATUS a04_battery_set_notify(PVOID context, ULONG tag, PBATTERY_NOTIFY notify)
{
    UNREFERENCED_PARAMETER(context); UNREFERENCED_PARAMETER(tag); UNREFERENCED_PARAMETER(notify);
    return STATUS_NOT_SUPPORTED;
}
NTSTATUS a04_battery_disable_notify(PVOID context)
{
    UNREFERENCED_PARAMETER(context);
    return STATUS_NOT_SUPPORTED;
}
