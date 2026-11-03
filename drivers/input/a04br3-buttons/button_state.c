#include "button_state.h"

uint8_t a04_buttons_decode(uint16_t keypad, uint16_t pmic)
{
    uint8_t result = 0;
    if (!(pmic & 0x0002u)) result |= 1u << A04_POWER;
    if (!(keypad & 0x0002u)) result |= 1u << A04_HOME;
    if (!(keypad & 0x0001u)) result |= 1u << A04_VOLUME_UP;
    if (!(pmic & 0x0004u)) result |= 1u << A04_VOLUME_DOWN;
    return result;
}

int a04_buttons_next(uint8_t desired, uint8_t delivered)
{
    unsigned i;
    for (i = 0; i < A04_BUTTON_COUNT; ++i)
        if ((desired ^ delivered) & (1u << i)) return (int)i;
    return -1;
}

uint8_t a04_buttons_ack(uint8_t delivered, unsigned button)
{
    return button < A04_BUTTON_COUNT ? (uint8_t)(delivered ^ (1u << button)) : delivered;
}
