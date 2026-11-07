/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ_COLD_PROFILE_H
#define A04_BQ_COLD_PROFILE_H
#include "charger.h"
#include "recovery.h"
/* Only a cold token from the checked STOPPED record is accepted. Caller owns
 * I2C2 and has already armed maintenance. No HiZ, reset, USB negotiation or
 * fault recovery. This installs the board profile at USB 100 mA only. */
struct a04_bq_cold_profile_result {
    struct a04_bq_snapshot before;
    unsigned stage;
    uint8_t readback[4];
    int write_attempted;
};
int a04_bq_cold_profile(struct a04_bq_owner *,const struct a04_bq_recovery_io *,
                       struct a04_bq_cold_profile_result *);
#endif
