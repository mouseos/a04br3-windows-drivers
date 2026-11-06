/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ_BATTERY_STATUS_H
#define A04_BQ_BATTERY_STATUS_H
/* Call only after a04_bq_step accepted the cached snapshot. Linux
 * charging_get_charging_status treats STAT 3 and 4 as charging. STAT 5 is
 * accepted by the owner as complete, not actively charging. */
static int a04_bq_status_charging(unsigned status_register)
{
    unsigned stat=(status_register>>4)&7u;
    return stat==3u || stat==4u;
}
#endif
