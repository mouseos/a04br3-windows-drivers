#ifndef MT8167_AUDIO_BROKER_POLICY_H
#define MT8167_AUDIO_BROKER_POLICY_H
#include <stdint.h>
/* Offline policy only: the actual parent must serialize every call with its
 * own lock and implement resource ownership, I/O, teardown and recovery. */
struct mt8167_broker_policy { uint32_t holder; int poisoned; };
int mt8167_broker_acquire(struct mt8167_broker_policy *p, uint32_t client);
int mt8167_broker_release(struct mt8167_broker_policy *p, uint32_t client,
                          int transaction_completed);
/* Recovery is a hardware operation performed by the real owner, not this API.
 * Start a fresh policy only after all old leases are revoked and recovery is
 * independently verified. Never use struct initialization as live recovery. */
#endif
