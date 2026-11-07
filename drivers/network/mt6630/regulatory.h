/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WIFI_REGULATORY_H
#define A04_WIFI_REGULATORY_H
#include <stdint.h>
/* Legacy 20MHz ERP subset. Original country/MAC come from checked NVRAM.
 * Channels14 and 5GHz are excluded. Source default table21 preserves the
 * original country bytes; it never invents a country or calibration. */
struct a04_regulatory {uint8_t domain[56],mac[6],country[2];unsigned permitted,active;};
enum {A04_REG_OK=0,A04_REG_ARGUMENT=1,A04_REG_VERSION=2,A04_REG_TX_VALID=3,
 A04_REG_MAC=4,A04_REG_MAP=5,A04_REG_BAND=6,A04_REG_GEOMETRY=7,A04_REG_EMPTY=8};
enum {A04_REG_COUNTRY_TABLE=1,A04_REG_INDEX_TABLE=2,A04_REG_CUSTOM=3,A04_REG_DEFAULT_TABLE=4};
struct a04_regulatory_inspection {
 uint16_t own1,peer1,own2,peer2;
 unsigned reason,selection,country_class,map,index,bad_subband;
};
int a04_regulatory_decode_inspect(const uint8_t [512],struct a04_regulatory *,struct a04_regulatory_inspection *);
int a04_regulatory_decode(const uint8_t [512],struct a04_regulatory *);
int a04_manufacture_submit(const uint8_t [512],const struct a04_regulatory *,void *,
 int (*set)(void *,unsigned,const uint8_t *,unsigned));
#endif
