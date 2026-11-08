/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include "../drivers/power/mt8167-thermal/first_read.h"
struct mock{unsigned calls;int32_t status;};
static int32_t collect(void*p){struct mock*m=p;m->calls++;return m->status;}
int main(void){struct a04_thermal_first_read s={0};struct mock m={0};assert(!s.attempted&&!m.calls);
 assert(!a04_thermal_collect_once(&s,&m,collect)&&m.calls==1);assert(!a04_thermal_collect_once(&s,&m,collect)&&m.calls==1);
 s=(struct a04_thermal_first_read){0};m=(struct mock){0,-7};assert(a04_thermal_collect_once(&s,&m,collect)==-7&&m.calls==1);m.status=0;
 assert(a04_thermal_collect_once(&s,&m,collect)==-7&&m.calls==1);puts("PASS first READ defers collection and caches exact one-shot error/success without retries; mock callback, no temperature success");}
