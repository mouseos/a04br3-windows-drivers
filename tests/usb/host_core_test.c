/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "host_core.h"
int main(void)
{
 struct a04_root_port p;struct a04_host_queue q;struct a04_transfer a,b,c;int ka,kb,kc;
 a04_port_init(&p);assert(a04_port_power(&p,1,0)<0);assert(a04_port_connect(&p,1)<0);
 assert(!a04_port_power(&p,1,1));assert(!a04_port_connect(&p,1));assert(p.change==A04_CHANGE_CONNECTION);
 assert(!a04_port_reset_begin(&p));assert(a04_port_reset_done(&p,A04_PORT_LOW_SPEED|A04_PORT_HIGH_SPEED)<0);
 assert(!a04_port_reset_done(&p,A04_PORT_HIGH_SPEED));assert((p.status&(A04_PORT_ENABLE|A04_PORT_HIGH_SPEED))==(A04_PORT_ENABLE|A04_PORT_HIGH_SPEED));
 assert(!a04_port_clear_change(&p,A04_CHANGE_CONNECTION));assert((p.change&A04_CHANGE_RESET)!=0);
 assert(!a04_port_overcurrent(&p,1));assert(!(p.status&(A04_PORT_POWER|A04_PORT_ENABLE|A04_PORT_CONNECTION)));assert(a04_port_power(&p,1,1)<0);
 assert(!a04_port_overcurrent(&p,0));assert(!a04_port_power(&p,1,1));assert(!a04_port_connect(&p,1));assert(!a04_port_reset_begin(&p));assert(!a04_port_connect(&p,0));assert(a04_port_reset_done(&p,0)<0);
 a04_queue_init(&q);assert(a04_queue_submit(&q,&a,&ka,10)<0);assert(!a04_queue_online(&q));
 assert(!a04_queue_submit(&q,&a,&ka,10));assert(!a04_queue_submit(&q,&b,&kb,5));assert(a04_queue_submit(&q,&c,&ka,5)<0);
 assert(a04_queue_start(&q)==&a);assert(!a04_queue_cancel(&q,&b));assert(b.state==A04_XFER_DONE && b.result==A04_RESULT_CANCELLED);
 assert(a04_queue_cancel(&q,&a)==1);assert(a04_queue_finish(&q,&a,A04_RESULT_OK,0,0)<0);assert(!a04_queue_finish(&q,&a,A04_RESULT_OK,2,1));assert(a.result==A04_RESULT_CANCELLED && a.actual==2);
 assert(!a04_queue_submit(&q,&b,&kb,5));assert(!a04_queue_submit(&q,&c,&kc,3));assert(a04_queue_start(&q)==&b);
 a04_queue_disconnect(&q);assert(a04_queue_online(&q)<0);assert(a04_queue_start(&q)==0);assert(a04_queue_finish(&q,&b,A04_RESULT_OK,0,0)<0);
 assert(!a04_queue_finish(&q,&b,A04_RESULT_OK,0,1));assert(b.result==A04_RESULT_DISCONNECTED);assert(!a04_queue_finish(&q,&c,A04_RESULT_DISCONNECTED,0,1));assert(!a04_queue_online(&q));
 return 0;
}
