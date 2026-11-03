/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "device.h"

struct mock { unsigned transfers, reads; int fail, release; };
static int write_byte(void *context, uint8_t value)
{
    struct mock *m = context; (void)value; ++m->transfers;
    return m->fail ? -1 : 0;
}
static int read_pair(void *context, uint8_t value[2])
{
    struct mock *m = context; ++m->transfers;
    if (m->fail) return -1;
    ++m->reads;
    if (m->release && m->reads % 3 == 0) { value[0] = 0; value[1] = 0; }
    else { value[0] = 0x3e; value[1] = 0x80; } /* synthetic ADC value 1000 */
    return 0;
}
static void delay_us(void *context, unsigned delay) { (void)context; (void)delay; }
int main(void)
{
    const struct a04_touch_calibration cal = {0,4095,0,4095,0,0,0};
    const struct a04_touch_pressure_policy pressure = {500,200,5,900000};
    struct mock m = {0};
    struct a04_bu21025_io io = {&m, write_byte, read_pair, delay_us};
    struct a04_touch_device device;
    uint8_t report[A04_TOUCH_REPORT_SIZE], down[A04_TOUCH_REPORT_SIZE];
    unsigned prior;
    assert(a04_touch_device_init(&device, &io, &cal, &pressure) == 0);
    assert(m.transfers == 3);
    assert(a04_touch_device_poll(&device, 0, 0, report) == A04_POLL_IDLE && m.transfers == 3);
    assert(a04_touch_device_poll(&device, 1, 10, report) == A04_POLL_REPORT);
    assert(report[1] == 1 && report[9] == 1 && m.transfers == 13);
    memcpy(down, report, sizeof(down)); prior = m.transfers;
    assert(a04_touch_device_poll(&device, 0, 11, report) == A04_POLL_REPORT);
    assert(m.transfers == prior + 10 && report[1] == 1);
    m.release = 1;
    assert(a04_touch_device_poll(&device, 0, 12, report) == A04_POLL_REPORT);
    assert(report[1] == 0 && memcmp(report+3, down+3, 4) == 0);
    prior = m.transfers; m.release = 0;
    assert(a04_touch_device_poll(&device, 0, 12, report) == A04_POLL_IDLE && m.transfers == prior);
    assert(a04_touch_device_poll(&device, 1, 12, report) == A04_POLL_REPORT);
    memcpy(down, report, sizeof(down)); m.fail = 1;
    assert(a04_touch_device_poll(&device, 1, 13, report) == A04_POLL_FAULT_REPORT);
    assert(report[1] == 0 && memcmp(report+3, down+3, 4) == 0);
    prior = m.transfers;
    assert(a04_touch_device_poll(&device, 1, 14, report) == A04_POLL_FAULT && m.transfers == prior);
    puts("touch device orchestration tests passed");
    return 0;
}
