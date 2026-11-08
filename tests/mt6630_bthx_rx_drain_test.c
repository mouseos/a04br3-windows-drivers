/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "../drivers/bluetooth/mt6630/rx_drain.h"
struct state {unsigned available,queued,waiting,pulls,delivered,fail,order;};
static void deliver(void*p){struct state*s=p;unsigned n=s->queued<s->waiting?s->queued:s->waiting;s->queued-=n;s->waiting-=n;s->delivered+=n;}
static int room(void*p){return ((struct state*)p)->queued<32;}
static int pull(void*p){struct state*s=p;s->pulls++;if(s->fail&&s->pulls==s->fail)return -1;if(!s->available)return 0;s->available--;s->queued++;s->order++;return 1;}
int main(void){
 struct state s={.available=40,.waiting=40};struct mt6630_rx_drain_io io={&s,deliver,room,pull};
 assert(mt6630_rx_drain(&io)==MT6630_RX_BUDGET&&s.pulls==32&&s.delivered==32&&s.available==8);
 assert(mt6630_rx_drain(&io)==MT6630_RX_EMPTY&&s.delivered==40&&s.order==40&&!s.queued);
 s=(struct state){.available=1,.queued=32};assert(mt6630_rx_drain(&io)==MT6630_RX_FULL&&!s.pulls&&s.available==1);
 s.waiting=1;assert(mt6630_rx_drain(&io)==MT6630_RX_FULL&&s.pulls==1&&s.queued==32&&!s.available&&s.delivered==1);
 s=(struct state){.available=10,.waiting=10,.fail=3};assert(mt6630_rx_drain(&io)==MT6630_RX_FAULT&&s.pulls==3&&s.delivered==2&&s.available==8);
 puts("PASS production bounded RX pump: burst32+8, full/nonconsuming, deliver-before-read, FIFO count, actual-error stop; mock broker only");return 0;
}
