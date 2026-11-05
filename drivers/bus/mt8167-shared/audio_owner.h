/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_AUDIO_OWNER_H
#define A04_AUDIO_OWNER_H
#include <stdint.h>
/* All operations run under the parent's shared resource lock. The platform
 * callbacks must own their hardware exclusively for the entire lease. A
 * failing callback may have partially changed hardware, so it quarantines
 * the owner. No recovery, power-off, or DMA drain is inferred. */
struct a04_audio_owner {
    void *context;
    int (*preflight)(void *, uint32_t);
    int (*top_clock_acquire)(void *, uint32_t);
    int (*top_clock_release)(void *);
    int (*clock_acquire)(void *, uint32_t);
    int (*codec_acquire_muted)(void *);
    /* Must independently establish device DMA and interrupt quiescence;
     * a caller assertion or disabled AFE bit alone is insufficient. */
    int (*verify_release)(void *);
    int (*codec_release_quiescent)(void *);
    int (*clock_release)(void *);
    int online, leased, poisoned;
};
int a04_audio_owner_acquire(struct a04_audio_owner *, uint32_t);
int a04_audio_owner_release(struct a04_audio_owner *);
void a04_audio_owner_stop(struct a04_audio_owner *);
#endif
