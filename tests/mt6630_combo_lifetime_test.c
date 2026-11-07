/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * The production broker's lock-protected admission state, exercised without
 * Windows or hardware. No fake WMT OFF/lease-release result is generated. */
#include <assert.h>
#include <stdio.h>
#include "../drivers/bluetooth/mt6630/broker_lifetime.h"
int main(void)
{
 struct a04_combo_client_file file={0};struct a04_combo_requests q={0};
 int owner,other,read,write,cleanup;unsigned kind;
 assert(!a04_combo_client_create(&file,&owner));
 assert(a04_combo_client_create(&file,&other));
 assert(a04_combo_client_controls(&file,&owner)&&!a04_combo_client_controls(&file,&other));
 /* A READ is executing, a WRITE was already queued, then last-handle
  * CLEANUP arrives. A single slot used to reject this cleanup as BUSY. */
 assert(!a04_combo_requests_put(&q,&read,0));
 assert(a04_combo_requests_take(&q,&kind)==&read&&!kind);
 assert(!a04_combo_requests_put(&q,&write,0));
 assert(a04_combo_client_cleanup(&file,&owner)==1);
 assert(!a04_combo_client_controls(&file,&owner));
 assert(!a04_combo_requests_put(&q,&cleanup,1));
 assert(a04_combo_client_cleanup(&file,&owner)==0); /* duplicate cannot replace pending IRP */
 assert(q.cleanup==&cleanup);
 assert(a04_combo_client_create(&file,&other));
 assert(a04_combo_requests_take(&q,&kind)==&write&&!kind);
 assert(a04_combo_requests_take(&q,&kind)==&cleanup&&kind);
 assert(!a04_combo_requests_pending(&q));
 /* Even a drained request queue cannot admit a replacement FileObject until
  * the I/O manager reports CLOSE for the retired file. */
 assert(a04_combo_client_create(&file,&other));
 a04_combo_client_closed(&file,&other);assert(file.file==&owner);
 a04_combo_client_closed(&file,&owner);assert(!file.file);
 assert(!a04_combo_client_create(&file,&other));
 /* Scheduler allocation failure does not remove cleanup from the queue.
  * Existing DPC can retry, or Halt can drain it once the worker has joined. */
 assert(a04_combo_client_cleanup(&file,&other)==1);
 assert(!a04_combo_requests_put(&q,&cleanup,1));
 assert(a04_combo_requests_pending(&q));assert(!a04_combo_client_controls(&file,&other));
 assert(a04_combo_requests_take(&q,&kind)==&cleanup&&kind);
 assert(!a04_combo_requests_pending(&q));
 assert(!a04_combo_requests_take(&q,&kind));
 puts("MT6630 production broker admission: queued/inflight cleanup, retirement, duplicate and OOM retention (synthetic): OK");
 return 0;
}
