/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ_D0_SESSION_H
#define A04_BQ_D0_SESSION_H
#include <stdint.h>
/* Private to the local-only diagnostic owner. No external broker clients may
 * retain pointers across these epochs. One fresh BQHR admission on first D0;
 * later epochs require the configured DEFAULT contract to complete. With the
 * readback flag this includes actual exit/resume masked observation; legacy
 * BQ14 only recorded write completion. This is
 * not independent verification of hardware DEFAULT or a firmware successor. */
struct a04_bq_d0_session { uint64_t generation; unsigned entered, reset_complete, failed; };
/* 1 initial, 2 resume, 0 rejected. Consumes admission before any BQ read.
 * Caller has synchronously stopped timer/closed endpoint under the bus lock
 * before reinitializing private per-epoch owner/broker storage on return 2. */
int a04_bq_d0_begin(struct a04_bq_d0_session *);
void a04_bq_d0_default_result(struct a04_bq_d0_session *, int write_result);
int a04_bq_d0_can_resume(const struct a04_bq_d0_session *);
#endif
