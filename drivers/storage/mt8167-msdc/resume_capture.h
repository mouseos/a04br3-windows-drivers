/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MSDC_RESUME_CAPTURE_H
#define A04_MSDC_RESUME_CAPTURE_H
#include "pio.h"
#include "resume_check.h"
/* The caller supplies a non-replayable generation and must own power, pins,
 * clocks, MMIO and card for the entire call. permitted must revoke immediately
 * on stop/removal; it is not itself proof of a functioning power provider.
 * Only CMD13, CMD10, CMD8 are sent. No reset, CMD6 or card-data write.
 * Failure leaves *out unchanged and the controller/card must not be reused
 * until recovered. This is not wired to a Windows resume callback. */
struct a04_capture_authority {
    void *context;
    int (*permitted)(void *, uint32_t);
    uint64_t (*now)(void *);
    uint64_t ticks;
    uint32_t generation;
    uint16_t rca;
};
enum a04_capture_result { A04_CAPTURE_OK, A04_CAPTURE_INVALID, A04_CAPTURE_REVOKED,
    A04_CAPTURE_UNSAFE, A04_CAPTURE_TIMEOUT, A04_CAPTURE_COMMAND, A04_CAPTURE_DATA,
    A04_CAPTURE_CARD_STATE, A04_CAPTURE_IDENTITY, A04_CAPTURE_MODE };
enum a04_capture_result a04_resume_capture(const struct a04_msdc_io *io,
    const struct a04_capture_authority *authority,uint32_t poll_limit,
    uint8_t extcsd_scratch[512],struct a04_resume_observation *out);
#endif
