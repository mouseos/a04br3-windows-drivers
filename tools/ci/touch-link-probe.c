/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Compile/link probe only. It deliberately refuses to load and does not
 * register a device or access hardware. */
#include <ntddk.h>
#include "../../drivers/input/bu21025/bu21025.h"
#include "../../drivers/input/bu21025/coordinates.h"
#include "../../drivers/input/bu21025/mt8167_adapter.h"

static volatile int probe_result;
static uint16_t read16(void *context, unsigned offset)
{
    UNREFERENCED_PARAMETER(context);
    return offset == 0x0c ? 1 : 0;
}
static void write16(void *context, unsigned offset, uint16_t value)
{
    UNREFERENCED_PARAMETER(context); UNREFERENCED_PARAMETER(offset); UNREFERENCED_PARAMETER(value);
}
static void delay_us(void *context, unsigned delay)
{
    UNREFERENCED_PARAMETER(context); UNREFERENCED_PARAMETER(delay);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    struct a04_i2c_io bus = {NULL, read16, write16, delay_us};
    struct a04_bu21025_mt8167 adapter = {&bus, 0x48, 0};
    struct a04_bu21025_io io;
    struct a04_bu21025_sample sample = {0};
    struct a04_touch_calibration calibration = {0, 4095, 0, 4095, 0, 0, 0};
    struct a04_touch_position position;
    UNREFERENCED_PARAMETER(driver); UNREFERENCED_PARAMETER(path);
    probe_result = a04_bu21025_mt8167_bind(&adapter, &io);
    probe_result = a04_bu21025_init(&io);
    probe_result = a04_bu21025_sample(&io, &sample);
    probe_result = a04_touch_map(&calibration, sample.x, sample.y, &position);
    return STATUS_NOT_SUPPORTED;
}
