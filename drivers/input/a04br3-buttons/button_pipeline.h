#ifndef A04BR3_BUTTON_PIPELINE_H
#define A04BR3_BUTTON_PIPELINE_H

#include <stdint.h>

#define A04_BUTTON_SOURCE_KEYPAD 1u
#define A04_BUTTON_SOURCE_PMIC 2u

struct a04_button_pipeline {
    uint8_t candidate, stable, known, delivered, synchronized;
    uint8_t samples[4];
};

/* Only synchronize to an externally established inbox-driver state. */
void a04_button_pipeline_init(struct a04_button_pipeline *p);
void a04_button_pipeline_sync(struct a04_button_pipeline *p, uint8_t delivered);
void a04_button_pipeline_invalidate(struct a04_button_pipeline *p);
/* Returns one Windows button index to toggle, or -1. Three consecutive valid
 * samples are required per button. A failed source pauses its buttons. */
int a04_button_pipeline_sample(struct a04_button_pipeline *p, uint16_t keypad,
                               uint16_t pmic, unsigned valid_sources);
/* Call only after a confirmed successful one-byte notification. */
void a04_button_pipeline_ack(struct a04_button_pipeline *p, unsigned button);

#endif
