/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "musb_async.h"
int mt_musb_admit(struct mt_musb_async *s,unsigned cookie,unsigned long long now,unsigned long long span)
{
 if(s->state!=MT_JOB_IDLE||!cookie||!span||span>50000000ull||now+span<now)return -1;
 s->epoch++;if(!s->epoch)s->epoch++;
 s->ticket=s->epoch;s->cookie=cookie;s->actual=0;s->deadline=now+span;s->state=MT_JOB_QUEUED;return 0;
}
int mt_musb_claim(struct mt_musb_async *s,unsigned *ticket)
{if(s->state!=MT_JOB_QUEUED)return -1;s->state=MT_JOB_RUNNING;*ticket=s->ticket;return 0;}
void mt_musb_cancel(struct mt_musb_async *s,unsigned cookie)
{
 if(s->cookie!=cookie)return;
 if(s->state==MT_JOB_DONE){s->last_cookie=cookie;s->last_toggle=s->toggle;s->state=MT_JOB_IDLE;}
 else if(s->state==MT_JOB_QUEUED||s->state==MT_JOB_RUNNING)s->state=MT_JOB_CANCEL;
}
int mt_musb_expired(const struct mt_musb_async *s,unsigned long long now)
{return (s->state==MT_JOB_RUNNING||s->state==MT_JOB_QUEUED)&&now>=s->deadline;}
int mt_musb_finish(struct mt_musb_async *s,unsigned ticket,unsigned actual,unsigned status,unsigned toggle,int proven)
{
 if(ticket!=s->ticket||(s->state!=MT_JOB_RUNNING&&s->state!=MT_JOB_CANCEL))return -1;
 if(!proven){s->state=MT_JOB_FAULT;return -1;}
 s->actual=actual;s->status=status;s->toggle=toggle;
 s->last_cookie=s->cookie;s->last_toggle=toggle;
 s->state=s->state==MT_JOB_CANCEL?MT_JOB_IDLE:MT_JOB_DONE;return 0;
}
int mt_musb_take(struct mt_musb_async *s,unsigned cookie,unsigned *actual,unsigned *status,unsigned *toggle)
{
 if(s->state!=MT_JOB_DONE||s->cookie!=cookie)return -1;
 *actual=s->actual;*status=s->status;*toggle=s->toggle;s->state=MT_JOB_IDLE;return 0;
}
