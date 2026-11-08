/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "../../drivers/usb/mt8167-usb20/musb_async.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
 struct mt_musb_async s={0};unsigned ticket,actual,status,toggle,i;
 assert(!mt_musb_admit(&s,1,100,20));assert(mt_musb_admit(&s,2,100,20));
 mt_musb_cancel(&s,2);assert(s.state==MT_JOB_QUEUED); /* stale close */
 assert(!mt_musb_claim(&s,&ticket));mt_musb_cancel(&s,1);
 assert(mt_musb_admit(&s,2,100,20)); /* cancel cannot reuse the hardware slot */
 assert(!mt_musb_finish(&s,ticket,4,9,1,1));assert(s.state==MT_JOB_IDLE);
 assert(mt_musb_take(&s,1,&actual,&status,&toggle)); /* aborted work never completes twice */
 assert(s.last_cookie==1&&s.last_toggle==1);
 for(i=0;i<100000;i++){
  assert(!mt_musb_admit(&s,i+2,100,20));assert(!mt_musb_claim(&s,&ticket));
  assert(!mt_musb_expired(&s,119)&&mt_musb_expired(&s,120));
  assert(mt_musb_finish(&s,ticket-1,0,0,0,1)); /* old epoch cannot publish */
  assert(!mt_musb_finish(&s,ticket,8,0,0,1));
  assert(!mt_musb_take(&s,i+2,&actual,&status,&toggle));assert(actual==8&&!status);
  assert(mt_musb_take(&s,i+2,&actual,&status,&toggle));
 }
 assert(mt_musb_admit(&s,1,~0ull-4,5));assert(mt_musb_admit(&s,0,0,1));
 assert(!mt_musb_admit(&s,1,0,1));assert(!mt_musb_claim(&s,&ticket));
 assert(mt_musb_finish(&s,ticket,0,0,0,0));assert(s.state==MT_JOB_FAULT);
 mt_musb_cancel(&s,1);assert(mt_musb_admit(&s,2,0,1));
 puts("MUSB_ASYNC_CANCEL_EPOCH_DEADLINE_STOP_FAILURE_PASS");return 0;
}
