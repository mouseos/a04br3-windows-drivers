/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "resource_lease.h"
struct fixture { int calls[32], n, fail; struct a04_resource_lease *lease; uint32_t generation; };
static int record(struct fixture *f,int value) { f->calls[f->n++]=value; return f->fail==value; }
static int acquire(void *c,enum a04_lease_resource r) { return record(c,10+(int)r); }
static int release(void *c,enum a04_lease_resource r) { return record(c,30+(int)r); }
static int irq(void *c) { struct fixture *f=c;assert(!a04_resource_lease_live(f->lease,f->generation));return record(c,20); }
static int io(void *c) { return record(c,21); }
int main(void)
{
 struct a04_resource_lease l={0};struct fixture f={{0},0,0,&l,0};
 struct a04_lease_ops ops={&f,acquire,release,irq,io};uint32_t g=0;int i;struct a04_wifi_owner owner={0};
 assert(!a04_resource_lease_acquire(&l,&ops,&g));f.generation=g;
 assert(a04_resource_lease_live(&l,g));assert(!a04_resource_lease_live(&l,g+1));
 assert(!a04_resource_lease_bind_owner(&l,&owner));assert(a04_resource_lease_same_owner(&l,&owner,g));
 {struct a04_wifi_owner other={0};assert(a04_resource_lease_bind_owner(&l,&other));assert(!a04_resource_lease_same_owner(&l,&other,g));}
 assert(!a04_resource_lease_same_owner(&l,&owner,g+1));
 assert(a04_resource_lease_acquire(&l,&ops,&g));assert(!a04_resource_lease_quiesce(&l,g));
 assert(l.state==A04_LEASE_QUIESCED && f.n==A04_LEASE_COUNT+2);
 assert(a04_resource_lease_release(&l,g+1));assert(!a04_resource_lease_release(&l,g));
 assert(f.calls[A04_LEASE_COUNT]==20 && f.calls[A04_LEASE_COUNT+1]==21);
 for(i=0;i<A04_LEASE_COUNT;i++)assert(f.calls[A04_LEASE_COUNT+2+i]==30+A04_LEASE_COUNT-1-i);
 assert(l.held==0 && l.state==A04_LEASE_STOPPED);assert(!a04_resource_lease_same_owner(&l,&owner,g));a04_wifi_revoke(&owner);
 assert(!a04_resource_lease_acquire(&l,&ops,&g));assert(g==2);f.generation=g;f.fail=21;
 assert(a04_resource_lease_stop(&l,g));assert(l.state==A04_LEASE_FAILED && l.held==((1u<<A04_LEASE_COUNT)-1u));
 assert(a04_resource_lease_stop(&l,g));assert(a04_resource_lease_acquire(&l,&ops,&g));
 {struct a04_resource_lease x={0};struct fixture y={{0},0,12,&x,0};struct a04_lease_ops z={&y,acquire,release,irq,io};
  g=99;assert(a04_resource_lease_acquire(&x,&z,&g));assert(g==99 && x.held==7 && x.state==A04_LEASE_FAILED && y.n==3);}
 for(i=0;i<A04_LEASE_COUNT;i++){
  struct a04_resource_lease x={0};struct fixture y={{0},0,10+i,&x,0};struct a04_lease_ops z={&y,acquire,release,irq,io};
  g=88;assert(a04_resource_lease_acquire(&x,&z,&g));assert(g==88 && x.held==((1u<<(i+1))-1u) && y.n==i+1);
 }
 for(i=0;i<A04_LEASE_COUNT;i++){
  struct a04_resource_lease x={0};struct fixture y={{0},0,0,&x,0};struct a04_lease_ops z={&y,acquire,release,irq,io};
  assert(!a04_resource_lease_acquire(&x,&z,&g));y.generation=g;y.fail=30+i;
  assert(a04_resource_lease_stop(&x,g));assert(x.state==A04_LEASE_FAILED && x.held==((1u<<(i+1))-1u));
 }
 puts("MT6630 resource acquisition, rundown and failure retention: OK");return 0;
}
