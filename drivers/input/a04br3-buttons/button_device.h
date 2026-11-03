#ifndef A04BR3_BUTTON_DEVICE_H
#define A04BR3_BUTTON_DEVICE_H
#include "button_pipeline.h"
#include <stdint.h>

/* notify returns 0 on confirmed delivery, 1 only when known not delivered,
 * otherwise an unknown outcome. All calls must be serialized by the owner. */
struct a04_button_device {
    struct a04_button_pipeline pipeline;
    void *context;
    int (*read_keypad)(void *, uint16_t *);
    int (*read_pmic)(void *, uint16_t *);
    int (*notify)(void *, uint8_t);
};
void a04_button_device_init(struct a04_button_device *device);
/* The owner must establish the endpoint's state before calling sync. */
void a04_button_device_sync(struct a04_button_device *device, uint8_t state);
/* A report is usable only after the owner independently establishes that no
 * old notification remains queued. Unknown, malformed and special states
 * invalidate synchronization and are never interpreted as released. */
int a04_button_device_sync_report(struct a04_button_device *device,
                                  const uint8_t *report, unsigned length,
                                  int queue_quiescent);
void a04_button_device_disconnect(struct a04_button_device *device);
/* Returns 1 when delivered, 0 when no event/retry-safe failure, -1 for an
 * unknown delivery outcome or invalid interface. Never implies wake support. */
int a04_button_device_poll(struct a04_button_device *device);
#endif
