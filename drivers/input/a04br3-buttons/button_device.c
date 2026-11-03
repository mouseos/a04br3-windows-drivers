/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "button_device.h"

void a04_button_device_init(struct a04_button_device *device)
{
    a04_button_pipeline_init(&device->pipeline);
}
void a04_button_device_sync(struct a04_button_device *device, uint8_t state)
{
    a04_button_pipeline_sync(&device->pipeline, state);
}
void a04_button_device_disconnect(struct a04_button_device *device)
{
    a04_button_pipeline_invalidate(&device->pipeline);
}
int a04_button_device_poll(struct a04_button_device *device)
{
    uint16_t keypad = 0, pmic = 0;
    unsigned valid = 0;
    int button, result;
    if (!device || !device->read_keypad || !device->read_pmic || !device->notify)
        return -1;
    if (!device->read_keypad(device->context, &keypad)) valid |= A04_BUTTON_SOURCE_KEYPAD;
    if (!device->read_pmic(device->context, &pmic)) valid |= A04_BUTTON_SOURCE_PMIC;
    button = a04_button_pipeline_sample(&device->pipeline, keypad, pmic, valid);
    if (button < 0) return 0;
    result = device->notify(device->context, (uint8_t)button);
    if (!result) { a04_button_pipeline_ack(&device->pipeline, (unsigned)button); return 1; }
    if (result == 1) return 0;
    a04_button_pipeline_invalidate(&device->pipeline);
    return -1;
}
