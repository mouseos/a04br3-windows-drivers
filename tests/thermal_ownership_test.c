/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include "ownership.h"
int main(void){struct a04_thermal_owner s={0};
 assert(a04_owner_activate(&s));s.resources=1;assert(a04_owner_activate(&s));
 s.touch_lease=1;assert(a04_owner_activate(&s));s.firmware_handoff=1;
 assert(!a04_owner_activate(&s));assert(a04_owner_activate(&s));
 a04_owner_revoke(&s);assert(!s.active&&!s.resources&&!s.touch_lease&&!s.firmware_handoff&&s.generation==1);
 assert(a04_owner_activate(&s));puts("AUXADC resource, touch and firmware ownership all required: OK");}
