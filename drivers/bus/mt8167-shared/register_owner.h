/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_REGISTER_OWNER_H
#define A04_SHARED_REGISTER_OWNER_H
#include <stdint.h>
/* Parent owns lifetime and holds one shared blocking lock across each entire
 * operation. A client must hold a PnP lifetime reference before calling.
 * Hardware callbacks must report failures. Not a PnP interface by itself. */
enum a04_shared_bank { A04_SHARED_TOP, A04_SHARED_INFRA, A04_SHARED_GPIO };
struct a04_register_owner {
    void *context;
    int (*lock)(void *); void (*unlock)(void *);
    int (*read32)(void *,enum a04_shared_bank,unsigned,uint32_t *);
    int (*write32)(void *,enum a04_shared_bank,unsigned,uint32_t);
    int online,poisoned;
};
int a04_register_owner_start(struct a04_register_owner *);
int a04_register_touch_update(struct a04_register_owner *,enum a04_shared_bank,unsigned,uint32_t,uint32_t);
int a04_register_touch_write(struct a04_register_owner *,enum a04_shared_bank,unsigned,uint32_t);
int a04_register_touch_input(struct a04_register_owner *,int *down);
/* Caller holds the shared parent lock, and has established platform-wide
 * exclusive ownership and retained power before using this diagnostic lease. */
struct a04_audio_top_clock { int held, restore_gated, poisoned; };
int a04_audio_top_clock_acquire(struct a04_register_owner *,struct a04_audio_top_clock *);
int a04_audio_top_clock_release(struct a04_register_owner *,struct a04_audio_top_clock *);
int a04_register_owner_stop(struct a04_register_owner *);
#endif
