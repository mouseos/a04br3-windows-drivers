/* SPDX-License-Identifier: MIT */
#ifndef A04_BROKER_WIRE_H
#define A04_BROKER_WIRE_H
#include "broker_mailbox.h"
#define A04_BROKER_WIRE_BYTES 48u
/* Fixed little-endian copied frames. A platform transport still has to
 * provide atomic copy, exclusion, cache maintenance and boot-local origin. */
int a04_broker_wire_request_encode(uint8_t *, unsigned, const struct a04_broker_request *);
int a04_broker_wire_request_decode(struct a04_broker_request *, const uint8_t *, unsigned);
int a04_broker_wire_response_encode(uint8_t *, unsigned, const struct a04_broker_response *);
int a04_broker_wire_response_decode(struct a04_broker_response *, const uint8_t *, unsigned);
#endif
