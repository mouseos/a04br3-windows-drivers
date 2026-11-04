/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "broker_client.h"
#include "owner_transfer.h"
#define NS 1000000000ULL
struct fixture { uint64_t now; int count, fail, mode; };
static uint64_t now(void *p) {return ((struct fixture *)p)->now;}
static int action(void *p) {struct fixture *f=p;++f->count;if(f->mode==1)f->now=10*NS;return f->count==f->fail;}
static int exchange(void *p,const struct a04_broker_request *q,struct a04_broker_response *r) {
 struct fixture *f=p;++f->count;r->epoch=q->epoch;r->sequence=q->sequence;r->result=A04_BROKER_OK;
 r->sample.voltage_mv=4000;r->sample.indicator_tenths_percent=525;r->sample.percent=50;
 if(f->mode==1)r->epoch++;
 if(f->mode==2)r->sequence++;
 if(f->mode==3)f->now=q->deadline;
 if(f->mode==4)r->sample.percent=99;
 if(f->mode==5)return -1;
 return 0;
}
int main(void) {
 struct fixture f;struct a04_owner_transfer s;struct a04_transfer_ops ops;
 struct a04_broker_client c;struct a04_client_transport t;struct a04_gauge_sample out,old;int i;
 for(i=0;i<=4;i++) {
  memset(&f,0,sizeof(f));memset(&s,0,sizeof(s));f.fail=i;
  ops.context=&f;ops.close_admission=action;ops.drain=action;ops.release=action;ops.acquire=action;ops.now_ns=now;
  if(i) {assert(a04_owner_transfer_run(&s,&ops,10*NS));assert(s.state==A04_TRANSFER_FAILED&&f.count==i);}
  else {assert(!a04_owner_transfer_run(&s,&ops,10*NS));assert(s.state==A04_TRANSFER_NEW&&f.count==4);}
  assert(a04_owner_transfer_run(&s,&ops,10*NS));
 }
 memset(&f,0,sizeof(f));memset(&s,0,sizeof(s));f.mode=1;assert(a04_owner_transfer_run(&s,&ops,10*NS)&&f.count==1);
 for(i=0;i<=5;i++) {
  memset(&f,0,sizeof(f));memset(&c,0,sizeof(c));memset(&out,0x5a,sizeof(out));old=out;f.mode=i;
  t.context=&f;t.now_ns=now;t.exchange=exchange;assert(!a04_broker_client_init(&c,&t,42,0));
  if(!i) {assert(!a04_broker_client_read(&c,10*NS,&out));assert(out.voltage_mv==4000&&c.sequence==1);}
  else {assert(a04_broker_client_read(&c,10*NS,&out));assert(c.stopped&&!memcmp(&out,&old,sizeof(out)));assert(a04_broker_client_read(&c,10*NS,&out)&&f.count==1);}
 }
 return 0;
}
