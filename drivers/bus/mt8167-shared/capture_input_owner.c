/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "capture_input_owner.h"
int a04_capture_input_acquire(struct a04_capture_input_owner *o,unsigned rate)
{
 int rc;
 if(!o||!o->online||rate!=48000||o->reserved||o->ready||o->poisoned||
    !o->reserve||!o->unreserve||!o->power_acquire||!o->power_release||
    !o->verify_drained||!o->io.analog_read||!o->io.analog_write||
    !o->io.afe_read||!o->io.afe_write)return -1;
 /* reserve is read-only with respect to hardware; failure changes no state. */
 if(o->reserve(o->context))return -1;
 o->reserved=1;
 /* May partially enable supplies even on failure; never auto-release them. */
 o->power_held=1;
 if(o->power_acquire(o->context,rate)){o->poisoned=1;return -2;}
 rc=a04_capture_codec_prepare(&o->codec,&o->io);
 if(rc){o->poisoned=1;return -2;}
 o->ready=1;return 0;
}
int a04_capture_input_release(struct a04_capture_input_owner *o)
{
 if(!o||!o->reserved||!o->power_held||!o->ready||o->poisoned||
    !o->verify_drained||!o->power_release||!o->unreserve)return -1;
 /* PnP stop must still be able to release after online becomes false.
  * No codec write or supply release before independent DMA/IRQ proof. */
 if(o->verify_drained(o->context))return -1;
 if(a04_capture_codec_stop(&o->codec,&o->io)){
  o->poisoned=1;return -2;
 }
 o->ready=0;
 if(o->power_release(o->context)){o->poisoned=1;return -2;}
 o->power_held=0;
 if(o->unreserve(o->context)){o->poisoned=1;return -2;}
 o->reserved=0;return 0;
}
void a04_capture_input_offline(struct a04_capture_input_owner *o)
{if(o)o->online=0;}
