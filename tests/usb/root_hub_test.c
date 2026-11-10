/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "root_hub.h"
int main(void)
{
 struct a04_root_port p;struct a04_hub h;struct a04_hub_action a,old;
 struct a04_hub_observation o={0};unsigned n;unsigned char b[16];
 unsigned char s[8]={0x23,3,8,0,1,0,0,0};
 a04_port_init(&p);assert(a04_hub_init(&h,&p,0)<0);assert(!a04_hub_init(&h,&p,5));
 assert(a04_hub_control(&h,s,0,0,&n,&a)==1);old=a;
 assert(!(p.status&A04_PORT_POWER));
 /* Requests cannot bypass qualification, ID, overcurrent or voltage checks. */
 o.endpoints_quiesced=1;o.vbus_verified=1;o.id_host=1;o.overcurrent_clear=1;
 assert(a04_hub_action_done(&h,&a,1,&o)<0);assert(!p.status);
 assert(a04_hub_control(&h,s,0,0,&n,&a)==1);o.qualified=1;
 assert(a04_hub_action_done(&h,&old,1,&o)<0);
 assert(!a04_hub_action_done(&h,&a,1,&o));assert(p.status&A04_PORT_POWER);
 assert(!a04_port_connect(&p,1));assert(a04_hub_status(&h,b,1)==0&&b[0]==2);
 s[0]=0xa3;s[1]=0;s[2]=0;s[6]=4;
 assert(!a04_hub_control(&h,s,b,16,&n,&a));assert(n==4&&b[0]==1&&b[1]==1&&b[2]==1);
 assert(!a04_hub_control(&h,s,b,16,&n,&a)&&b[2]==1); /* status doesn't clear */
 s[6]=3;assert(a04_hub_control(&h,s,b,16,&n,&a)<0);s[6]=4;s[4]=2;
 assert(a04_hub_control(&h,s,b,16,&n,&a)<0);s[4]=1;
 s[0]=0x23;s[1]=1;s[2]=16;s[6]=0;
 assert(!a04_hub_control(&h,s,0,0,&n,&a));assert(a04_hub_status(&h,b,1)==1&&b[0]==0);
 s[1]=3;s[2]=4;assert(a04_hub_control(&h,s,0,0,&n,&a)==1);
 o.reset_asserted_ms=49;assert(a04_hub_action_done(&h,&a,1,&o)<0);assert(!(p.status&A04_PORT_ENABLE));
 assert(a04_hub_control(&h,s,0,0,&n,&a)==1);o.reset_asserted_ms=50;
 assert(!a04_hub_action_done(&h,&a,1,&o));assert(p.status&A04_PORT_ENABLE);
 assert(p.change&A04_CHANGE_RESET);
 s[1]=1;s[2]=1;assert(a04_hub_control(&h,s,0,0,&n,&a)==A04_HUB_PENDING);
 o.endpoints_quiesced=0;assert(a04_hub_action_done(&h,&a,1,&o)<0&&(p.status&A04_PORT_ENABLE));
 assert(a04_hub_control(&h,s,0,0,&n,&a)==A04_HUB_PENDING);o.endpoints_quiesced=1;
 assert(!a04_hub_action_done(&h,&a,1,&o)&&!(p.status&A04_PORT_ENABLE)&&(p.status&A04_PORT_POWER));
 s[2]=18;assert(!a04_hub_control(&h,s,0,0,&n,&a));
 s[1]=3;s[2]=4;assert(a04_hub_control(&h,s,0,0,&n,&a)==A04_HUB_PENDING);
 assert(!a04_hub_action_done(&h,&a,1,&o)&&(p.status&A04_PORT_ENABLE));
 s[2]=8;assert(a04_hub_control(&h,s,0,0,&n,&a)==1);a04_hub_invalidate(&h);
 assert(a04_hub_action_done(&h,&a,1,&o)<0);
 assert(!a04_port_overcurrent(&p,1));assert(!(p.status&A04_PORT_POWER));
 assert(a04_hub_control(&h,s,0,0,&n,&a)==1);assert(a04_hub_action_done(&h,&a,1,&o)<0);
 s[1]=1;assert(a04_hub_control(&h,s,0,0,&n,&a)==1);
 assert(a04_hub_action_done(&h,&a,1,&o)<0); /* off must also be verified */
 assert(a04_hub_control(&h,s,0,0,&n,&a)==1);o.vbus_off_verified=1;
 assert(!a04_hub_action_done(&h,&a,1,&o));assert(p.status&A04_PORT_OVERCURRENT);
 s[0]=0xa0;s[1]=6;s[2]=0;s[3]=0x29;s[4]=0;s[6]=9;
 assert(!a04_hub_control(&h,s,b,16,&n,&a));assert(n==9&&b[2]==1&&b[3]==9&&b[5]==5&&b[8]==255);
 s[6]=4;assert(!a04_hub_control(&h,s,b,16,&n,&a)&&n==4);
 assert(a04_hub_control(&h,s,b,3,&n,&a)<0);
 return 0;
}
