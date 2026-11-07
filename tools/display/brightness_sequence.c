/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "brightness_sequence.h"
static int readback(const struct a04_brightness_ops *o,unsigned expected,const char *phase,unsigned *flags)
{
 unsigned i,value;int r;
 for(i=0;i<4;i++) {
  value=~0u;r=o->read(o->context,&value);
  if(o->record(o->context,phase,expected,value,r)){*flags|=4;return -1;}
  if(r)return r;
  if(value==expected)return 0;
  if(i<3)o->pause(o->context);
 }
 return -1;
}
unsigned a04_brightness_once(const struct a04_brightness_ops *o)
{
 unsigned original=0,flags=0,i;const unsigned levels[2]={30,70};int r;
 if(!o || !o->snapshot || !o->set || !o->read || !o->record || !o->pause)return 8;
 r=o->snapshot(o->context,&original);
 if(r || original<1 || original>100) {
  if(o->record(o->context,"preflight-failed",0,original,r))flags|=4;
  return flags|8;
 }
 /* Durable restoration checkpoint must precede any possibly effective Set. */
 if(o->record(o->context,"original-percent",original,original,0))return 4|8;
 for(i=0;i<2;i++) {
  r=o->set(o->context,levels[i]);
  if(o->record(o->context,"set-percent",levels[i],~0u,r))flags|=4;
  if(r || flags || readback(o,levels[i],"read-percent",&flags)){flags|=1;break;}
 }
 /* A failed/timed-out Set may still have applied. Never skip restoration. */
 r=o->set(o->context,original);
 if(o->record(o->context,"restore-set",original,~0u,r))flags|=4;
 if(r)flags|=2;
 if(readback(o,original,"restore-read",&flags))flags|=2;
 if(o->record(o->context,"result-flags",flags,0,0))flags|=4;
 return flags;
}
