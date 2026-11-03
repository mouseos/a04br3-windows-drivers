/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_EINT_H
#define A04_MT8167_EINT_H
#include <stdint.h>
#define A04_EINT_LINES 169u
#define A04_EINT_BANKS 6u

/* The platform must own and lock the shared EINT controller. Register access
 * callbacks must include required MMIO ordering; these helpers do not acquire
 * interrupts, configure pinmux/debounce, or implement wake handling. */
struct a04_eint_io {
    void *context;
    uint32_t (*read32)(void *context, unsigned offset);
    void (*write32)(void *context, unsigned offset, uint32_t value);
};
enum a04_eint_type { A04_EDGE_RISING, A04_EDGE_FALLING, A04_LEVEL_HIGH, A04_LEVEL_LOW };
/* Configuration acknowledges a pending event on this line. The owner must
 * quiesce its source and account for events that can arrive during changes. */
int a04_eint_configure(const struct a04_eint_io *io, unsigned line, enum a04_eint_type type);
int a04_eint_mask(const struct a04_eint_io *io, unsigned line, int masked);
int a04_eint_ack(const struct a04_eint_io *io, unsigned line);
int a04_eint_pending(const struct a04_eint_io *io, unsigned bank,
                     uint32_t owned, uint32_t *pending);
#endif
