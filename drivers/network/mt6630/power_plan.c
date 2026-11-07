/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "power_plan.h"
void a04_wifi_power_fault(struct a04_wifi_power *s) {if(s){s->phase=A04_WIFI_POISON;s->generation++;}}
int a04_wifi_power_begin(struct a04_wifi_power *s,uint64_t now,int host,int shared,int combo,int firmware)
{
 if(!s||s->phase!=A04_WIFI_OFF||!now||!host||!shared||!combo||!firmware)return -1;
 s->since=now;s->phase=A04_WIFI_WAIT_RAIL;return 0;
}
int a04_wifi_power_rail_settled(struct a04_wifi_power *s,uint64_t now,uint64_t min_delay)
{
 if(!s||s->phase!=A04_WIFI_WAIT_RAIL||!min_delay)return -1;
 if(now<s->since){a04_wifi_power_fault(s);return -1;}
 if(now-s->since<min_delay)return 1;
 s->phase=A04_WIFI_RELEASE_RESET;return 0;
}
int a04_wifi_power_reset_released(struct a04_wifi_power *s,uint64_t now,int operation_succeeded)
{
 if(!s||s->phase!=A04_WIFI_RELEASE_RESET)return -1;
 if(now<s->since||!operation_succeeded){a04_wifi_power_fault(s);return -1;}
 s->since=now;s->phase=A04_WIFI_WAIT_RESET;return 0;
}
int a04_wifi_power_ready(struct a04_wifi_power *s,uint64_t now,uint64_t min_delay)
{
 if(!s||s->phase!=A04_WIFI_WAIT_RESET||!min_delay)return -1;
 if(now<s->since){a04_wifi_power_fault(s);return -1;}
 if(now-s->since<min_delay)return 1;
 s->phase=A04_WIFI_READY;return 0;
}
