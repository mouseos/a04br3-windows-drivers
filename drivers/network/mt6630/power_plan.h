/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_POWER_PLAN_H
#define A04_MT6630_POWER_PLAN_H
#include <stdint.h>
/* Policy state only. No pins, clocks, PMIC or firmware access. The upstream
 * combo owner supplies each qualified operation and monotonic timestamps. */
enum a04_wifi_phase { A04_WIFI_OFF, A04_WIFI_WAIT_RAIL, A04_WIFI_RELEASE_RESET, A04_WIFI_WAIT_RESET, A04_WIFI_READY, A04_WIFI_POISON };
struct a04_wifi_power { enum a04_wifi_phase phase;uint64_t since;unsigned generation; };
int a04_wifi_power_begin(struct a04_wifi_power *,uint64_t,int,int,int,int);
int a04_wifi_power_rail_settled(struct a04_wifi_power *,uint64_t,uint64_t);
int a04_wifi_power_reset_released(struct a04_wifi_power *,uint64_t,int);
int a04_wifi_power_ready(struct a04_wifi_power *,uint64_t,uint64_t);
void a04_wifi_power_fault(struct a04_wifi_power *);
#endif
