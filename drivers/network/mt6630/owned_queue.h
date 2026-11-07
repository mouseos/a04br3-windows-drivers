/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_OWNED_QUEUE_H
#define A04_OWNED_QUEUE_H
#include "native_limits.h"
/* Caller holds its queue lock. Capacity includes the worker-owned item;
 * completion callbacks may reenter admission before that item is released. */
struct a04_owned_queue {void *items[A04_NATIVE_QUEUE];unsigned head,count,inflight;};
static inline int a04_queue_put(struct a04_owned_queue *q,void *p)
{if(!p||q->count+q->inflight>=A04_NATIVE_QUEUE)return -1;q->items[(q->head+q->count)%A04_NATIVE_QUEUE]=p;q->count++;return 0;}
static inline void *a04_queue_take(struct a04_owned_queue *q)
{void *p;if(!q->count||q->inflight)return 0;p=q->items[q->head];q->items[q->head]=0;q->head=(q->head+1)%A04_NATIVE_QUEUE;q->count--;q->inflight=1;return p;}
static inline void a04_queue_release(struct a04_owned_queue *q){q->inflight=0;}
static inline void *a04_queue_remove(struct a04_owned_queue *q,unsigned index)
{void *p;if(index>=q->count)return 0;p=q->items[(q->head+index)%A04_NATIVE_QUEUE];for(unsigned i=index;i+1<q->count;i++)q->items[(q->head+i)%A04_NATIVE_QUEUE]=q->items[(q->head+i+1)%A04_NATIVE_QUEUE];q->items[(q->head+--q->count)%A04_NATIVE_QUEUE]=0;return p;}
#endif
