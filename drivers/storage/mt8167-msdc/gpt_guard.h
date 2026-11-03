/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_GPT_GUARD_H
#define A04BR3_GPT_GUARD_H
#include <stdint.h>

/* Incrementally verify the primary GPT before restricting diagnostic writes. */
struct a04_gpt_guard {
    uint32_t crc, expected_crc, blocks, matches, first, last, usable_first, usable_last;
    uint32_t esp_matches, esp_first, esp_last;
    uint32_t range_first[128], range_last[128], range_count;
    int valid;
};
int a04_gpt_guard_begin(struct a04_gpt_guard *, const uint8_t header[512], uint32_t sectors);
int a04_gpt_guard_feed(struct a04_gpt_guard *, const uint8_t entries[512]);
int a04_gpt_guard_finish(const struct a04_gpt_guard *, uint32_t *first, uint32_t *last);
int a04_gpt_guard_esp(const struct a04_gpt_guard *, uint32_t *first, uint32_t *last);
int a04_gpt_guard_range(uint32_t first, uint32_t last, uint64_t lba, uint32_t count);
#endif
