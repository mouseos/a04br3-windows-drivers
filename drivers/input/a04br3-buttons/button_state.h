#ifndef A04BR3_BUTTON_STATE_H
#define A04BR3_BUTTON_STATE_H

#include <stdint.h>

/* GPIOBUTTONS_BUTTON_TYPE ordering from Microsoft's Windows 8.1 interface. */
enum a04_button { A04_POWER, A04_HOME, A04_VOLUME_UP, A04_VOLUME_DOWN, A04_BUTTON_COUNT };

/* Active-low debounced registers.  Return bits indexed by enum a04_button. */
uint8_t a04_buttons_decode(uint16_t keypad_mem1, uint16_t pmic_chrstatus);

/* The inbox injection API toggles a button per event.  Advance only after a
 * successful write, so transient failures can be retried without losing an
 * edge.  Starting with delivered=0 synchronizes a held button on startup. */
int a04_buttons_next(uint8_t desired, uint8_t delivered);
uint8_t a04_buttons_ack(uint8_t delivered, unsigned button);

#endif
