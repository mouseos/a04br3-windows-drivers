#ifndef A04BR3_BUTTON_HARDWARE_H
#define A04BR3_BUTTON_HARDWARE_H
#include "button_device.h"
#include "pmic_read.h"

/* The platform owns keypad and WACS2 resources, their clocks and locks. */
struct a04_button_hardware {
    struct a04_button_device device;
    struct a04_pmic_read pmic;
    void *context;
    int (*keypad_read16)(void *, unsigned, uint16_t *);
    int (*notify)(void *, uint8_t);
};
void a04_button_hardware_init(struct a04_button_hardware *hardware);
#endif
