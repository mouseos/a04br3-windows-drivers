/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include "ownership.h"
#include "resources.h"
int main(void){struct a04_thermal_owner s={0};
 struct a04_thermal_resource r[4]={
  {0x1100d000,0x1100d000,4096,4096,1,1,1,1},
  {0x11003000,0x11003000,4096,4096,1,1,1,1},
  {0x10018000,0x10018000,4096,4096,1,1,1,1},
  {0x10009000,0x10009000,4096,4096,1,1,1,1}};
 assert(!a04_thermal_resources_validate(r,4));assert(a04_thermal_resources_validate(r,3));
 r[1].exclusive=0;assert(a04_thermal_resources_validate(r,4));r[1].exclusive=1;
 r[1].raw_exclusive=0;assert(a04_thermal_resources_validate(r,4));r[1].raw_exclusive=1;
 r[1].raw++;assert(a04_thermal_resources_validate(r,4));r[1].raw--;
 r[1]=r[0];assert(a04_thermal_resources_validate(r,4));
 assert(a04_owner_activate(&s));s.resources=1;assert(a04_owner_activate(&s));
 s.touch_lease=1;assert(a04_owner_activate(&s));s.firmware_handoff=1;
 assert(!a04_owner_activate(&s));assert(a04_owner_activate(&s));
 a04_owner_revoke(&s);assert(!s.active&&!s.resources&&!s.touch_lease&&!s.firmware_handoff&&s.generation==1);
 assert(a04_owner_activate(&s));
 s.resources=s.touch_lease=s.firmware_handoff=1;assert(!a04_owner_activate(&s));
 a04_owner_quarantine(&s);assert(s.quarantined && !s.active);
 a04_owner_revoke(&s);s.resources=s.touch_lease=s.firmware_handoff=1;
 assert(a04_owner_activate(&s));
 puts("AUXADC resource, touch and firmware ownership all required: OK");}
