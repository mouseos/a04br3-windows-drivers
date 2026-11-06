/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "host_core.h"
#if A04_MUSB_HOST_CORE_EXPERIMENT
void a04_port_init(struct a04_root_port *p) { p->status=0;p->change=0; }
static void port_drop(struct a04_root_port *p)
{
 if(p->status&A04_PORT_CONNECTION)p->change|=A04_CHANGE_CONNECTION;
 if(p->status&A04_PORT_ENABLE)p->change|=A04_CHANGE_ENABLE;
 p->status&=~(A04_PORT_CONNECTION|A04_PORT_ENABLE|A04_PORT_RESET|A04_PORT_LOW_SPEED|A04_PORT_HIGH_SPEED);
}
int a04_port_power(struct a04_root_port *p,int on,int verified)
{
 if(!p || (on!=0 && on!=1) || (on && (!verified || (p->status&A04_PORT_OVERCURRENT))))return -1;
 if(on)p->status|=A04_PORT_POWER;else {port_drop(p);p->status&=~A04_PORT_POWER;}return 0;
}
int a04_port_connect(struct a04_root_port *p,int connected)
{
 if(!p || (connected!=0 && connected!=1) || (connected && !(p->status&A04_PORT_POWER)))return -1;
 if(connected) {if(!(p->status&A04_PORT_CONNECTION))p->change|=A04_CHANGE_CONNECTION;p->status|=A04_PORT_CONNECTION;}
 else port_drop(p);
 return 0;
}
int a04_port_reset_begin(struct a04_root_port *p)
{
 if(!p || (p->status&(A04_PORT_POWER|A04_PORT_CONNECTION|A04_PORT_OVERCURRENT|A04_PORT_RESET))!=(A04_PORT_POWER|A04_PORT_CONNECTION))return -1;
 if(p->status&A04_PORT_ENABLE)p->change|=A04_CHANGE_ENABLE;
 p->status&=~(A04_PORT_ENABLE|A04_PORT_LOW_SPEED|A04_PORT_HIGH_SPEED);p->status|=A04_PORT_RESET;return 0;
}
int a04_port_reset_done(struct a04_root_port *p,unsigned speed)
{
 if(!p || !(p->status&A04_PORT_RESET) || (speed!=0 && speed!=A04_PORT_LOW_SPEED && speed!=A04_PORT_HIGH_SPEED))return -1;
 p->status&=~A04_PORT_RESET;p->status|=A04_PORT_ENABLE|speed;p->change|=A04_CHANGE_RESET|A04_CHANGE_ENABLE;return 0;
}
int a04_port_overcurrent(struct a04_root_port *p,int active)
{
 if(!p || (active!=0 && active!=1))return -1;
 if(!!(p->status&A04_PORT_OVERCURRENT)!=active)p->change|=A04_CHANGE_OVERCURRENT;
 if(active) {port_drop(p);p->status=(p->status&~A04_PORT_POWER)|A04_PORT_OVERCURRENT;}else p->status&=~A04_PORT_OVERCURRENT;return 0;
}
int a04_port_clear_change(struct a04_root_port *p,unsigned mask)
{ if(!p || !mask || (mask&~A04_CHANGES))return -1;p->change&=~mask;return 0; }
void a04_queue_init(struct a04_host_queue *q) { unsigned i;q->count=0;q->online=0;for(i=0;i<A04_QUEUE_CAPACITY;i++)q->items[i]=0; }
int a04_queue_online(struct a04_host_queue *q) { if(!q || q->count)return -1;q->online=1;return 0; }
int a04_queue_submit(struct a04_host_queue *q,struct a04_transfer *t,void *token,unsigned length)
{
 unsigned i;if(!q || !q->online || !t || !token || q->count==A04_QUEUE_CAPACITY)return -1;
 for(i=0;i<q->count;i++)if(q->items[i]==t || q->items[i]->token==token)return -1;
 t->token=token;t->state=A04_XFER_PENDING;t->result=0;t->length=length;t->actual=0;q->items[q->count++]=t;return 0;
}
struct a04_transfer *a04_queue_start(struct a04_host_queue *q)
{ if(!q || !q->online || !q->count || q->items[0]->state!=A04_XFER_PENDING)return 0;q->items[0]->state=A04_XFER_ACTIVE;return q->items[0]; }
int a04_queue_cancel(struct a04_host_queue *q,struct a04_transfer *t)
{
 unsigned i;if(!q || !t)return -1;for(i=0;i<q->count;i++)if(q->items[i]==t)break;if(i==q->count)return -1;
 if(t->state==A04_XFER_ACTIVE){t->state=A04_XFER_ABORTING;t->result=A04_RESULT_CANCELLED;return 1;}
 if(t->state!=A04_XFER_PENDING)return -1;
 return a04_queue_finish(q,t,A04_RESULT_CANCELLED,0,1);
}
int a04_queue_finish(struct a04_host_queue *q,struct a04_transfer *t,unsigned result,unsigned actual,int quiesced)
{
 unsigned i,j;if(!q || !t || result>A04_RESULT_FAILED || actual>t->length)return -1;
 for(i=0;i<q->count;i++)if(q->items[i]==t)break;
 if(i==q->count)return -1;
 if((t->state==A04_XFER_ACTIVE || t->state==A04_XFER_ABORTING) && !quiesced)return -1;
 if(t->state==A04_XFER_ABORTING)result=t->result;
 else if(t->state!=A04_XFER_ACTIVE && !(t->state==A04_XFER_PENDING && result!=A04_RESULT_OK))return -1;
 t->actual=actual;t->result=result;t->state=A04_XFER_DONE;for(j=i+1;j<q->count;j++)q->items[j-1]=q->items[j];q->items[--q->count]=0;return 0;
}
void a04_queue_disconnect(struct a04_host_queue *q)
{
 unsigned i;if(!q)return;q->online=0;for(i=0;i<q->count;i++){struct a04_transfer *t=q->items[i];if(t->state==A04_XFER_ACTIVE){t->state=A04_XFER_ABORTING;t->result=A04_RESULT_DISCONNECTED;}}
 /* Pending requests remain queued for explicit completion by the caller. */
}
#endif
