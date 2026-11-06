/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WIFI_REGULATORY_H
#define A04_WIFI_REGULATORY_H
#include <stdint.h>
/* Legacy 20MHz ERP subset. Original country/MAC come from checked NVRAM.
 * Channels14 and 5GHz are excluded; unknown country never becomes world. */
struct a04_regulatory {uint8_t domain[56],mac[6],country[2];unsigned permitted,active;};
int a04_regulatory_decode(const uint8_t [512],struct a04_regulatory *);
int a04_manufacture_submit(const uint8_t [512],const struct a04_regulatory *,void *,
 int (*set)(void *,unsigned,const uint8_t *,unsigned));
#endif
