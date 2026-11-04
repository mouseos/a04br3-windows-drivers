/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "power_plan.h"
int main(void) {
 struct a04_wifi_power s={0};
 assert(a04_wifi_power_begin(&s,1,1,1,1,0));assert(!a04_wifi_power_begin(&s,100,1,1,1,1));
 assert(a04_wifi_power_reset_released(&s,199,1));assert(a04_wifi_power_rail_settled(&s,199,100)==1);assert(!a04_wifi_power_rail_settled(&s,200,100));
 assert(!a04_wifi_power_reset_released(&s,200,1));assert(a04_wifi_power_ready(&s,229,30)==1);assert(!a04_wifi_power_ready(&s,230,30));
 a04_wifi_power_fault(&s);assert(s.phase==A04_WIFI_POISON);assert(a04_wifi_power_begin(&s,300,1,1,1,1));
 s.phase=A04_WIFI_OFF;assert(!a04_wifi_power_begin(&s,500,1,1,1,1));assert(a04_wifi_power_rail_settled(&s,499,100)<0&&s.phase==A04_WIFI_POISON);
 puts("Wi-Fi power prerequisites, deadlines and poison: OK");return 0;
}
