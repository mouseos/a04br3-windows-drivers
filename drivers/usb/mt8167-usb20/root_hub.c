/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "root_hub.h"
#if A04_MUSB_HOST_CORE_EXPERIMENT
static unsigned word(const unsigned char *p){return p[0]|((unsigned)p[1]<<8);}
static void put(unsigned char *p,unsigned x){p[0]=(unsigned char)x;p[1]=(unsigned char)(x>>8);}
int a04_hub_init(struct a04_hub *h,struct a04_root_port *p,unsigned pg)
{
 if(!h||!p||!pg||pg>255)return -1;
 h->port=p;h->action=0;h->generation=0;h->active_generation=0;h->power_good_2ms=(unsigned char)pg;return 0;
}
void a04_hub_invalidate(struct a04_hub *h)
{if(h){++h->generation;h->action=0;h->active_generation=0;}}
static int action(struct a04_hub *h,unsigned kind,struct a04_hub_action *a)
{
 if(h->action||!a)return A04_HUB_STALL;
 if(++h->generation==0)++h->generation;
 h->action=kind;h->active_generation=h->generation;
 a->kind=kind;a->generation=h->generation;return A04_HUB_PENDING;
}
int a04_hub_control(struct a04_hub *h,const unsigned char s[8],unsigned char *b,
 unsigned cap,unsigned *actual,struct a04_hub_action *a)
{
 unsigned v,i,n,j,len=0;unsigned char data[9]={0};
 if(!h||!h->port||!s||!actual)return A04_HUB_STALL;
 *actual=0;if(a){a->kind=0;a->generation=0;}
 v=word(s+2);i=word(s+4);n=word(s+6);
 if(n>cap||(n&&!b))return A04_HUB_STALL;
 if(s[0]==0xa0&&s[1]==6&&v==0x2900&&!i){
  /* One port, individual power/overcurrent reporting, removable port.
   * Power-good delay is supplied by the board, not guessed here. */
  data[0]=9;data[1]=0x29;data[2]=1;data[3]=9;data[5]=h->power_good_2ms;data[8]=255;len=9;
 }else if(s[0]==0xa0&&s[1]==0&&!v&&!i&&n==4){len=4;
 }else if(s[0]==0xa3&&s[1]==0&&!v&&i==1&&n==4){
  put(data,h->port->status);put(data+2,h->port->change);len=4;
 }else if(s[0]==0x20&&(s[1]==1||s[1]==3)&&v<=1&&!i&&!n){return A04_HUB_OK;
 }else if(s[0]==0x23&&i==1&&!n){
  if(s[1]==1){
   unsigned mask=0;
   if(v==16)mask=A04_CHANGE_CONNECTION;
   if(v==17)mask=A04_CHANGE_ENABLE;
   if(v==19)mask=A04_CHANGE_OVERCURRENT;
   if(v==20)mask=A04_CHANGE_RESET;
   if(mask)return a04_port_clear_change(h->port,mask)?A04_HUB_STALL:A04_HUB_OK;
   if(v==8)return action(h,A04_HUB_POWER_OFF,a);
   /* Enable/suspend need verified hardware operations, not software bit flips. */
  }else if(s[1]==3){
   if(v==8)return action(h,A04_HUB_POWER_ON,a);
   if(v==4){
    unsigned need=A04_PORT_POWER|A04_PORT_CONNECTION;
    if((h->port->status&(need|A04_PORT_OVERCURRENT|A04_PORT_RESET))!=need)return A04_HUB_STALL;
    return action(h,A04_HUB_RESET,a);
   }
  }
  return A04_HUB_STALL;
 }else return A04_HUB_STALL;
 if(len>n)len=n;
 for(j=0;j<len;j++)b[j]=data[j];
 *actual=len;return A04_HUB_OK;
}
int a04_hub_action_done(struct a04_hub *h,const struct a04_hub_action *a,int success,
 const struct a04_hub_observation *o)
{
 unsigned kind;
 if(!h||!h->port||!a||!h->action||a->kind!=h->action||a->generation!=h->active_generation)return -1;
 kind=h->action;h->action=0;h->active_generation=0;
 if(!success||!o||!o->endpoints_quiesced)return -1;
 if(kind==A04_HUB_POWER_OFF)return (o->vbus_off_verified||o->source_off_verified)?a04_port_power(h->port,0,0):-1;
 if(!o->qualified||!o->id_host||!o->overcurrent_clear||!o->vbus_verified)return -1;
 if(kind==A04_HUB_POWER_ON)return a04_port_power(h->port,1,1);
 if(kind!=A04_HUB_RESET||o->reset_asserted_ms<50||
  (o->speed!=0&&o->speed!=A04_PORT_LOW_SPEED&&o->speed!=A04_PORT_HIGH_SPEED))return -1;
 if(a04_port_reset_begin(h->port))return -1;
 return a04_port_reset_done(h->port,o->speed);
}
int a04_hub_status(const struct a04_hub *h,unsigned char *b,unsigned cap)
{
 if(!h||!h->port||!b||cap<1)return -1;
 b[0]=h->port->change?2:0;return b[0]?A04_HUB_OK:A04_HUB_PENDING;
}
#endif
