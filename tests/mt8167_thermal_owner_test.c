/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "thermal_owner.h"
int main(void){struct a04_shared_thermal_owner s={0};uint32_t id=0;
 s.online=1;assert(a04_shared_thermal_acquire(&s,&id));s.firmware_released=1;assert(a04_shared_thermal_acquire(&s,&id));
 s.adc_exclusive=1;assert(!a04_shared_thermal_acquire(&s,&id)&&id==1);assert(a04_shared_thermal_acquire(&s,&id));
 assert(!a04_shared_thermal_valid(&s,2));assert(!a04_shared_thermal_ownership_verified(&s,1));assert(!a04_shared_thermal_begin(&s,1));
 assert(a04_shared_thermal_ownership_verified(&s,1));assert(!a04_shared_thermal_ownership_verified(&s,2));
 s.firmware_released=0;assert(!a04_shared_thermal_ownership_verified(&s,1));s.firmware_released=1;
 s.adc_exclusive=0;assert(!a04_shared_thermal_ownership_verified(&s,1));s.adc_exclusive=1;
 assert(a04_shared_thermal_release(&s,1));assert(a04_shared_thermal_acquire(&s,&id));
 a04_shared_thermal_end(&s);assert(!a04_shared_thermal_ownership_verified(&s,1));s.clocks_ready=1;assert(a04_shared_thermal_release(&s,1));
 assert(!a04_shared_thermal_quiescent(&s,1));assert(a04_shared_thermal_begin(&s,1));assert(!a04_shared_thermal_release(&s,1));
 assert(!a04_shared_thermal_acquire(&s,&id)&&id==2);a04_shared_thermal_stop(&s);
 assert(s.poisoned&&!s.online&&!a04_shared_thermal_valid(&s,id));s.online=s.firmware_released=s.adc_exclusive=1;
 assert(a04_shared_thermal_acquire(&s,&id));puts("thermal shared owner admission, generation and stop: OK");return 0;}
