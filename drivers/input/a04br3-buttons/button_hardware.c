/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "button_hardware.h"

static int keypad_read(void *context, uint16_t *value)
{
    struct a04_button_hardware *h = context;
    return h->keypad_read16 ? h->keypad_read16(h->context, 0x04u, value) : -1;
}
static int pmic_read(void *context, uint16_t *value)
{
    struct a04_button_hardware *h = context;
    return a04_pmic_chrstatus(&h->pmic, value);
}
static int notify(void *context, uint8_t button)
{
    struct a04_button_hardware *h = context;
    return h->notify ? h->notify(h->context, button) : -1;
}
void a04_button_hardware_init(struct a04_button_hardware *hardware)
{
    hardware->device.context = hardware;
    hardware->device.read_keypad = keypad_read;
    hardware->device.read_pmic = pmic_read;
    hardware->device.notify = notify;
    a04_button_device_init(&hardware->device);
}
