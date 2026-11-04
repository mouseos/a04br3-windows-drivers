#ifndef MT8167_CLOCK_OWNER_H
#define MT8167_CLOCK_OWNER_H
#include <stdint.h>
/* Policy for a real parent, not an MMIO driver. The parent owns the lock,
 * protects object lifetime and implements a trustworthy clock observation.
 * Existing firmware consumers must be included in its ownership contract. */
struct mt8167_clock_owner {
    void *context;
    int (*lock)(void *);
    void (*unlock)(void *);
    /* Return 0 only if the exclusive parent has verified this exact rate. */
    int (*verify_rate)(void *, uint32_t);
    uint32_t rate;
    uint32_t clients;
    int online;
    int poisoned;
};
#ifdef __cplusplus
extern "C" {
#endif
int mt8167_clock_acquire(struct mt8167_clock_owner *, unsigned client, uint32_t rate);
int mt8167_clock_release(struct mt8167_clock_owner *, unsigned client);
int mt8167_clock_stop(struct mt8167_clock_owner *);
#ifdef __cplusplus
}
#endif
#endif
