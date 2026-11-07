/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_HI556_IDENTITY_H
#define A04_HI556_IDENTITY_H
#include <stdint.h>
#ifndef A04_HI556_IDENTITY_EXPERIMENT
#define A04_HI556_IDENTITY_EXPERIMENT 0
#endif
#if A04_HI556_IDENTITY_EXPERIMENT != 0 && A04_HI556_IDENTITY_EXPERIMENT != 1
#error Identity experiment must be zero or one
#endif
#define A04_HI556_I2C_BUS 1u
#define A04_HI556_I2C_ADDRESS 0x28u
#define A04_HI556_CHIP_ID 0x0556u
/* This is an internal future-owner contract, not an implemented Windows
 * interface. All calls must be PASSIVE and under the sole controller owner.
 * begin grants a powered/clocked main-camera lease and controller-wide lock,
 * shared with MMA. Failure has no retained lease; no private MMIO owner is
 * permitted. A successful begin must be paired with finish, even on failure.
 * finish drains/aborts uncertain I/O before releasing the lock; a failed
 * finish quarantines resources and must not power-cycle a shared consumer. */
struct a04_hi556_io {
 void *context;
 int32_t (*begin)(void *,unsigned bus,unsigned address,uint64_t *lease);
 int (*live)(void *,uint64_t lease);
 /* Each operation must complete with STOP. Address write contains only the
  * two-byte register pointer, never a sensor register value. No WRRD merge. */
 int32_t (*address_stop)(void *,uint64_t,const uint8_t pointer[2],unsigned *completed);
 int32_t (*read_stop)(void *,uint64_t,uint8_t *value,unsigned *completed);
 int32_t (*finish)(void *,uint64_t);
};
enum a04_hi556_result {
 A04_HI556_OK=0,A04_HI556_DISABLED,A04_HI556_INVALID,
 A04_HI556_BEGIN_FAILED,A04_HI556_LEASE_LOST,A04_HI556_IO_FAILED,
 A04_HI556_SHORT_IO,A04_HI556_ID_MISMATCH,A04_HI556_FINISH_FAILED
};
enum a04_hi556_step {A04_HI556_NONE,A04_HI556_HIGH_ADDRESS,A04_HI556_HIGH_READ,
 A04_HI556_LOW_ADDRESS,A04_HI556_LOW_READ};
struct a04_hi556_observation {
 unsigned step,bytes_read,id_observed;
 uint16_t chip_id;
 int32_t transport_status,finish_status;
};
/* Returns identity only; never claims frames, calibration or streaming.
 * No retries, address scan, bus timing, power or sensor-value writes. */
enum a04_hi556_result a04_hi556_identify(const struct a04_hi556_io *,struct a04_hi556_observation *);
#endif
