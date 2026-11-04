/* SPDX-License-Identifier: MIT */
#ifndef A04_RENDER_SHARED_SIZE_H
#define A04_RENDER_SHARED_SIZE_H
#include <stdint.h>
#include <stddef.h>
/* Offline check of one CPU-synchronized little-endian saved-size word.
 * Valid only as an observation: it cannot constrain the preceding store,
 * prevent GPU accesses, prove the hardware register value or authorize kick.
 * Requires a completed fence and CPU cache synchronization by the caller. */
struct a04_render_shared_size_result { uint32_t blocks,dwords,bytes; };
int a04_render_check_saved_shared_size(const uint8_t *state,size_t size,
    struct a04_render_shared_size_result *out);
#endif
