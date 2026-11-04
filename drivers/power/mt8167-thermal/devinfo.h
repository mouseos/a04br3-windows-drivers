/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_MT8167_DEVINFO_H
#define A04_MT8167_DEVINFO_H
#include <stddef.h>
#include <stdint.h>
struct a04_devinfo_blob { const uint8_t *bytes; size_t size; };
/* Read the LK atag,devinfo property supplied by this boot, not a backup. */
int a04_devinfo_atag_read(void *, unsigned, uint32_t *);
#endif
