/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "power_bridge.h"
struct fake {int valid,stable,release;unsigned resets;};
static int valid(void *p,unsigned g){(void)g;return ((struct fake *)p)->valid;}
static int stable(void *p){return ((struct fake *)p)->stable;}
static int release(void *p){struct fake *f=p;f->resets++;return f->release;}
int main(void)
{
 struct a04_wifi_owner o={0,1,1,1,1,1,0,0};struct fake f={1,1,1,0};
 struct a04_wifi_power_bridge b={&o,{0},&f,valid,stable,release,0,10,3};
 assert(!a04_wifi_admit(&o));assert(!a04_wifi_power_start(&b,100));
 assert(a04_wifi_power_step(&b,109)==1 && !f.resets);
 assert(a04_wifi_power_step(&b,110)==1 && f.resets==1);
 assert(a04_wifi_power_step(&b,112)==1 && a04_wifi_power_step(&b,113)==0);
 o=(struct a04_wifi_owner){0,1,1,1,1,1,0,0};b.plan=(struct a04_wifi_power){0};f.resets=0;
 assert(!a04_wifi_admit(&o));assert(!a04_wifi_power_start(&b,100));f.valid=0;
 assert(a04_wifi_power_step(&b,110)<0 && !f.resets && o.poisoned);
 puts("Power bridge lease, waits and failure stop: OK");return 0;
}
