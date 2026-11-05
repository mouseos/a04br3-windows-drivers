/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ_ADMISSION_H
#define A04_BQ_ADMISSION_H
#include "charger.h"
/* Caller must establish exclusive I2C2, boot-local record origin, physical
 * counter continuity, armed maintenance and power/fault responsibility. A
 * valid record, epoch or checksum alone proves none of these requirements.
 * Sample now_ns before reading the physical counter. No I2C access here. */
int a04_bq_admission_prepare(struct a04_bq_owner *,const uint8_t *record,unsigned size,
                             uint64_t now_ns,uint64_t counter,uint64_t frequency,
                             uint64_t lifetime,uint64_t epoch);
#endif
