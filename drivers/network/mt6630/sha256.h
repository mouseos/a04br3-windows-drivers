/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHA256_H
#define A04_SHA256_H
#include <stddef.h>
#include <stdint.h>
/* SHA-256 for firmware identity, not signature verification. */
int a04_sha256(const uint8_t *,size_t,uint8_t [32]);
#endif
