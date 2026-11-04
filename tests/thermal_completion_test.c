/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include "completion.h"
struct env { uint32_t raw[3]; int eligible,ready,paused,busy,bad; };
static int qual(void *p){return ((struct env *)p)->eligible;}
static int ready(void *p){return ((struct env *)p)->ready;}
static int rd(void *p,uint32_t a,uint32_t *v){struct env *e=p;
 switch(a-0x1100d000u){case 0:*v=15;break;case 0x3c:*v=e->paused?2:0;break;case 0x68:*v=e->bad?0:0x1100300c;break;case 0x74:*v=0x11003008;break;case 0x78:case 0x7c:*v=0x11003040;break;case 0x418:*v=e->busy?0x10000:0;break;case 0x90:case 0x94:case 0x98:*v=e->raw[(a-0x1100d090)/4];break;default:return -1;}return 0;}
int main(void){struct env e={{0xda1,0xda1,0xda1},1,1,0,0,0};struct a04_completion c;uint32_t out[3]={1,2,3};
 struct a04_thermal_hw h={&e,qual,0,ready,rd,0,0};
 assert(!a04_completion_start(&c,100,100));assert(a04_completion_poll(&c,&h,100,30,out)==0);assert(out[0]==1);
 e.raw[0]=3000;e.raw[1]=3001;assert(a04_completion_poll(&c,&h,105,30,out)==0);e.raw[2]=3002;
 assert(a04_completion_poll(&c,&h,106,30,out)==1);assert(out[0]==3000&&out[2]==3002);
 assert(a04_completion_poll(&c,&h,137,30,out)==0);assert(a04_completion_poll(&c,&h,207,30,out)==-1);
 assert(!a04_completion_start(&c,1,50));e.paused=1;assert(a04_completion_poll(&c,&h,2,30,out)==-1);assert(!c.initialized);
 e.paused=0;assert(!a04_completion_start(&c,1,50));e.bad=1;assert(a04_completion_poll(&c,&h,2,30,out)==-1);
 puts("thermal synthetic-to-real completion evidence and stale rejection: OK");return 0;}
