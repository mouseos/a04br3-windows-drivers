/* SPDX-License-Identifier: MIT */
#ifndef A04_BROKER_HANDOFF_H
#define A04_BROKER_HANDOFF_H
#include "broker_mailbox.h"
#include "owner_transfer.h"
/* Same-clock-domain transfer only. Caller holds the old owner's I2C lock,
 * has quiesced client admission, and ensures no timer or other CPU accesses
 * the old owner during the transfer. Platform callbacks must prove exclusion,
 * receiver readiness and lifetime, not perform BQ IO. Neither transfer state
 * nor destination may be published until this function succeeds; acquire is
 * a reservation only. Destination is fresh and must not access I2C earlier.
 * Failure after an
 * attempted transfer stops both sides; it does not restore hardware safety. */
int a04_broker_handoff(struct a04_broker_endpoint *,struct a04_bq_owner *destination,
                       struct a04_owner_transfer *,const struct a04_transfer_ops *,uint64_t deadline);
#endif
