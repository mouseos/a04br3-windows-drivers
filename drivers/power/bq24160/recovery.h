/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ_RECOVERY_H
#define A04_BQ_RECOVERY_H
#include <stdint.h>
/* This interface does not prove any platform precondition. The caller holds
 * the shared I2C lock, has armed watchdog maintenance and must independently
 * establish all five conditions before the first transaction. */
struct a04_bq_recovery_proof {
    int exclusive_bus, firmware_stopped, worker_armed;
    int battery_can_supply_system, board_profile_and_usb_limit_valid;
};
struct a04_bq_recovery_io {
    void *context;
    uint64_t (*now_ns)(void *);
    int (*read)(void *,uint8_t,uint8_t *);
    int (*write)(void *,uint8_t,uint8_t);
};
/* ACK is an I2C transfer result, not independent readback. On any write
 * failure the hardware may have accepted the transaction despite the error. */
enum a04_bq_hiz_outcome {
    A04_BQ_HIZ_NOT_ATTEMPTED, A04_BQ_HIZ_ASSERT_UNCERTAIN,
    A04_BQ_HIZ_ASSERT_ACK, A04_BQ_HIZ_CLEAR_UNCERTAIN, A04_BQ_HIZ_CLEAR_ACK
};
struct a04_bq_recovery_result {
    unsigned stage; uint8_t before[8], after_battery; int write_attempted;
    enum a04_bq_hiz_outcome hiz_outcome;
};
/* BQ24160 BATOVP only; no reset, retries, profile programming, CD GPIO or
 * post-failure access. Zero means the status read no longer reports BATOVP;
 * it does not establish charging or guarantee that the hardware is safe. */
int a04_bq_recover_batovp(const struct a04_bq_recovery_proof *,const struct a04_bq_recovery_io *,
                          uint64_t deadline,struct a04_bq_recovery_result *);
#endif
