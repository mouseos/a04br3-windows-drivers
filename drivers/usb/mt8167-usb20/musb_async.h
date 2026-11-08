/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_MUSB_ASYNC_H
#define MT8167_MUSB_ASYNC_H
/* Portable ownership machine. Caller serializes every operation. No OS/URB
 * pointers are stored here. Worker operates only on a driver-owned shadow. */
#define MT_MUSB_SHADOW_BYTES 16384u
#define MT_JOB_IDLE 0u
#define MT_JOB_QUEUED 1u
#define MT_JOB_RUNNING 2u
#define MT_JOB_CANCEL 3u
#define MT_JOB_DONE 4u
#define MT_JOB_FAULT 5u
struct mt_musb_async {
 unsigned state,epoch,ticket,cookie,actual,status,toggle,last_cookie,last_toggle;
 unsigned long long deadline;
};
int mt_musb_admit(struct mt_musb_async *,unsigned,unsigned long long,unsigned long long);
int mt_musb_claim(struct mt_musb_async *,unsigned *);
void mt_musb_cancel(struct mt_musb_async *,unsigned);
int mt_musb_expired(const struct mt_musb_async *,unsigned long long);
int mt_musb_finish(struct mt_musb_async *,unsigned,unsigned,unsigned,unsigned,int);
int mt_musb_take(struct mt_musb_async *,unsigned,unsigned *,unsigned *,unsigned *);
#endif
