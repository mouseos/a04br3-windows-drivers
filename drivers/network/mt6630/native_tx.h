/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_TX_H
#define A04_NATIVE_TX_H
#include <stddef.h>
#include <stdint.h>
/* Strict non-QoS, unfragmented ToDS data frame, RFC1042 LLC/SNAP only.
 * Verifies receiver/AP and transmitter/local MAC before converting. No
 * cryptography, retransmit, QoS, sequence synthesis or NDIS buffer access. */
int a04_native_tx_to_ethernet(const uint8_t *mpdu,size_t bytes,
 const uint8_t ap[6],const uint8_t local[6],uint8_t *ethernet,size_t capacity,size_t *written);
#endif
