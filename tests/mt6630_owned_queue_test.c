/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "owned_queue.h"
int main(void)
{
 struct a04_owned_queue q={0};unsigned value[130];void *seen[64];
 for(unsigned i=0;i<64;i++){value[i]=i;assert(!a04_queue_put(&q,&value[i]));}
 assert(a04_queue_put(&q,&value[64]));
 /* A completion can reenter Send: the in-flight item still counts until
  * its owner returns. No slot may be reused while that callback runs. */
 assert(a04_queue_take(&q)==&value[0]);assert(!a04_queue_take(&q));
 assert(a04_queue_put(&q,&value[64]));assert(q.count==63&&q.inflight==1);
 a04_queue_release(&q);assert(!a04_queue_put(&q,&value[64]));
 /* Cancel a middle and head item, preserving FIFO and exactly-one owner. */
 assert(a04_queue_remove(&q,30)==&value[31]);assert(a04_queue_remove(&q,0)==&value[1]);
 assert(!a04_queue_remove(&q,64));
 unsigned n=0;while(q.count){void *p=a04_queue_take(&q);assert(p);for(unsigned i=0;i<n;i++)assert(seen[i]!=p);seen[n++]=p;a04_queue_release(&q);}
 assert(n==62&&!q.inflight&&!a04_queue_take(&q));
 /* Wrap and stop/drain: no admission outside the caller's lock/gate. */
 for(unsigned round=0;round<100;round++){
  for(unsigned i=0;i<64;i++)assert(!a04_queue_put(&q,&value[i]));
  for(unsigned i=0;i<64;i++){assert(a04_queue_take(&q)==&value[i]);a04_queue_release(&q);}
 }
 puts("Owned TX queue capacity, completion reentry, cancel FIFO and drain: OK");return 0;
}
