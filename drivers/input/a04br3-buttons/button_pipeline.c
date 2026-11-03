#include "button_pipeline.h"
#include "button_state.h"

void a04_button_pipeline_init(struct a04_button_pipeline *p)
{
    unsigned i;
    p->candidate = p->stable = p->known = p->delivered = p->synchronized = 0;
    for (i = 0; i < 4; ++i) p->samples[i] = 0;
}

void a04_button_pipeline_sync(struct a04_button_pipeline *p, uint8_t delivered)
{
    p->delivered = delivered & 0x0fu;
    p->synchronized = 1;
}

void a04_button_pipeline_invalidate(struct a04_button_pipeline *p)
{
    unsigned i;
    p->synchronized = 0;
    p->known = 0;
    for (i = 0; i < 4; ++i) p->samples[i] = 0;
}

int a04_button_pipeline_sample(struct a04_button_pipeline *p, uint16_t keypad,
                               uint16_t pmic, unsigned valid_sources)
{
    uint8_t decoded = a04_buttons_decode(keypad, pmic);
    unsigned i;
    for (i = 0; i < 4; ++i) {
        uint8_t bit = (uint8_t)(1u << i);
        unsigned source = (i == A04_HOME || i == A04_VOLUME_UP) ?
                          A04_BUTTON_SOURCE_KEYPAD : A04_BUTTON_SOURCE_PMIC;
        if (!(valid_sources & source)) {
            p->known &= (uint8_t)~bit;
            p->samples[i] = 0;
            continue;
        }
        if ((p->candidate & bit) != (decoded & bit)) {
            p->candidate = (uint8_t)((p->candidate & ~bit) | (decoded & bit));
            p->samples[i] = 1;
        } else if (p->samples[i] < 3) ++p->samples[i];
        if (p->samples[i] == 3) {
            p->stable = (uint8_t)((p->stable & ~bit) | (p->candidate & bit));
            p->known |= bit;
        }
    }
    if (!p->synchronized) return -1;
    for (i = 0; i < 4; ++i) {
        uint8_t bit = (uint8_t)(1u << i);
        if ((p->known & bit) && ((p->stable ^ p->delivered) & bit)) return (int)i;
    }
    return -1;
}

void a04_button_pipeline_ack(struct a04_button_pipeline *p, unsigned button)
{
    if (p->synchronized && button < 4) p->delivered ^= (uint8_t)(1u << button);
}
