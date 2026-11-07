/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_NVRAM_SECTOR_H
#define A04_MT6630_NVRAM_SECTOR_H
#include "nvram.h"
/* Read-only adapter for a validated, uniquely identified NVRAM partition on
 * the correct physical disk. Caller owns serialization and the context; the
 * sector callback must complete synchronously and fill exactly 512 bytes. */
struct a04_nvram_sector_reader {
 void *context;
 int (*read_sector)(void *, uint64_t, uint8_t [512]);
 uint64_t first_lba, sector_count;
 uint8_t scratch[512];
};
int a04_nvram_sector_init(struct a04_nvram_sector_reader *, struct a04_nvram_reader *,
 void *, int (*)(void *,uint64_t,uint8_t [512]), uint64_t, uint64_t, uint64_t);
#endif
