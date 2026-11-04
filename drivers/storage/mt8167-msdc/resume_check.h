/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MSDC_RESUME_CHECK_H
#define A04_MSDC_RESUME_CHECK_H
#include <stdint.h>
/* Validation only. The producer must obtain these from the same powered,
 * exclusively owned device after complete platform/card initialization.
 * generation is a nonzero monotonic power-transition generation supplied by
 * that owner. Widths are normalized to 1/4/8 bits; partition is the decoded
 * PARTITION_ACCESS value, not the raw EXT_CSD byte. Boolean evidence must be
 * exactly 0 or 1. This does not configure hardware, acquire resources, or
 * reopen I/O, and a VALID result is necessary but not sufficient to resume. */
struct a04_resume_observation {
    uint32_t generation, sectors, card_status;
    uint16_t rca;
    uint8_t cid[16];
    uint8_t host_width, card_width, partition, cache_ctrl;
    uint8_t power_owned, clock_stable, pio_mode, dma_idle, events_masked;
};
enum a04_resume_check { A04_RESUME_VALID, A04_RESUME_INVALID,
    A04_RESUME_GENERATION, A04_RESUME_OWNER, A04_RESUME_CARD_STATE,
    A04_RESUME_IDENTITY, A04_RESUME_MODE };
enum a04_resume_check a04_resume_validate(const struct a04_resume_observation *baseline,
                                           const struct a04_resume_observation *current);
#endif
