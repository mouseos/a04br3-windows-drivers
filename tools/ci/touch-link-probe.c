/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Compile/link probe only. It deliberately refuses to load and does not
 * register a device or access hardware. */
#include <ntddk.h>
#include "../../drivers/input/bu21025/bu21025.h"
#include "../../drivers/input/bu21025/coordinates.h"
#include "../../drivers/input/bu21025/mt8167_adapter.h"
#include "../../drivers/input/bu21025/touch_hid.h"
#include "../../drivers/input/bu21025/contact.h"
#include "../../drivers/input/bu21025/pipeline.h"

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
    struct a04_touch_position position = {0};
    struct a04_touch_report_state state = {0};
    struct a04_touch_pressure_policy pressure = {1, 1, 0, 1};
    struct a04_touch_pipeline pipeline;
    uint8_t report[A04_TOUCH_REPORT_SIZE], descriptor[A04_TOUCH_DESCRIPTOR_CAPACITY];
    uint8_t feature[A04_TOUCH_FEATURE_SIZE];
    size_t descriptor_length;
    UNREFERENCED_PARAMETER(driver); UNREFERENCED_PARAMETER(path);
    probe_result = a04_bu21025_mt8167_bind(&adapter, &io);
    probe_result = a04_bu21025_init(&io);
    probe_result = a04_bu21025_sample(&io, &sample);
    probe_result = a04_touch_classify(&pressure, &sample);
    probe_result = a04_touch_map(&calibration, sample.x, sample.y, &position);
    probe_result = a04_touch_descriptor(1, 1, descriptor, sizeof(descriptor), &descriptor_length);
    probe_result = a04_touch_report(&state, 1, position.x, position.y, 0, report);
    probe_result = a04_touch_max_contacts(feature);
    probe_result = a04_touch_pipeline_init(&pipeline, &calibration, &pressure);
    probe_result = a04_touch_pipeline_sample(&pipeline, &sample, 0, report);
    probe_result = a04_touch_pipeline_fault(&pipeline, 0, report);
    return STATUS_NOT_SUPPORTED;
}
