/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_IDENTITY_H
#define A04_MT6630_IDENTITY_H
#include <stddef.h>
#include <stdint.h>
/* A CIS snapshot is supplied by the future SD bus owner. No bus I/O here. */
struct a04_sdio_identity { uint16_t vendor, device; unsigned found; };
enum a04_cis_result { A04_CIS_OK=0,A04_CIS_TRUNCATED=-1,A04_CIS_MALFORMED=-2,A04_CIS_ID_MISSING=-3,A04_CIS_MISMATCH=-4 };
int a04_mt6630_cis(const uint8_t *,size_t,struct a04_sdio_identity *);
#endif
