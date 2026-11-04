/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MSDC_COLD_INIT_H
#define A04_MSDC_COLD_INIT_H
#include "resume_capture.h"
/* PASSIVE_LEVEL only. Caller must hold exclusive non-revocable resource and
 * I/O ownership until this function returns. prepare_valid must verify powered
 * rails, reset, pinmux and <=400 kHz one-bit host setup for this generation.
 * A callback result alone cannot prove those platform guarantees. This code
 * is unconnected to PnP and must not be used to open a startup/resume gate. */
struct a04_cold_authority {
    void *context;
    int (*prepare_valid)(void *,uint32_t);
    uint64_t (*now)(void *);
    void (*delay_us)(void *,uint32_t);
    uint64_t ticks;
    uint32_t generation;
    uint16_t rca;
};
enum a04_cold_result { A04_COLD_OK, A04_COLD_INVALID, A04_COLD_REVOKED,
    A04_COLD_UNSAFE, A04_COLD_TIMEOUT, A04_COLD_COMMAND, A04_COLD_OCR,
    A04_COLD_CAPTURE, A04_COLD_CID_MISMATCH };
/* Changes card state with CMD0/1/2/3/9/7/16, then observes via CMD13/10/8/13.
 * Never writes card data or CMD6. On failure controller/card are untrusted,
 * output remains unchanged, and caller must not reuse without full recovery. */
enum a04_cold_result a04_emmc_cold_init(const struct a04_msdc_io *,
    const struct a04_cold_authority *,uint32_t poll_limit,uint8_t scratch[512],
    struct a04_resume_observation *out);
#endif
