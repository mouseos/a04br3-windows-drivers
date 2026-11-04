/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "lifetime.h"
#include "identity.h"
static int ready(struct a04_wifi_life *s)
{ return s && s->state==A04_WIFI_LIFE_IO && s->owner && s->owner->active &&
 !s->owner->poisoned && s->owner->generation==s->generation; }
static void fault(struct a04_wifi_life *s)
{ s->state=A04_WIFI_LIFE_FAILED;s->transport.ready=0;s->transport.poisoned=1;
 s->direct.poisoned=1;if(s->owner && !s->owner->poisoned)a04_wifi_fault(s->owner); }
int a04_wifi_life_begin(struct a04_wifi_life *s,struct a04_wifi_owner *owner,
 const uint8_t *cis,size_t bytes,const struct a04_sdio_direct *direct,
 const struct a04_sdio_bus *bus,const struct a04_wifi_life_ops *ops)
{
 struct a04_sdio_identity id;
 if(!s || s->state!=A04_WIFI_LIFE_OFF || !owner || !owner->active || owner->poisoned ||
 !direct || direct->owner!=owner || direct->generation!=owner->generation || direct->poisoned || !direct->request ||
 !bus || !bus->request || !ops || !ops->mask_and_drain_irq || !ops->drain_io || !ops->unmap_host || !ops->release_lease ||
 a04_mt6630_cis(cis,bytes,&id))return -1;
 s->owner=owner;s->generation=owner->generation;s->verified_block_functions=0;s->direct=*direct;s->ops=*ops;
 s->transport.bus=*bus;s->transport.ready=1;s->transport.poisoned=0;s->state=A04_WIFI_LIFE_IO;return 0;
}
int a04_wifi_life_cmd52(struct a04_wifi_life *s,unsigned write,unsigned fn,unsigned raw,unsigned addr,uint8_t *data)
{
 int r;if(!ready(s))return -1;r=a04_sdio_direct52(&s->direct,write,fn,raw,addr,data);
 if(r==-2 || !ready(s)){fault(s);return -2;}return r;
}
int a04_wifi_life_block_verified(struct a04_wifi_life *s,unsigned fn,unsigned size)
{
 if(!ready(s) || !fn || fn>7 || size!=512)return -1;
 s->verified_block_functions|=1u<<fn;return 0;
}
int a04_wifi_life_cmd53(struct a04_wifi_life *s,unsigned write,unsigned fn,unsigned incr,unsigned addr,uint8_t *data,size_t length)
{
 int r;if(!ready(s) || (!incr && length>=512 && (fn>7 || !(s->verified_block_functions&(1u<<fn)))))return -1;r=a04_sdio_transfer(&s->transport,write,fn,incr,addr,data,length);
 if(r==-2 || !ready(s)){fault(s);return -2;}return r;
}
int a04_wifi_life_stop(struct a04_wifi_life *s)
{
 if(!s || (s->state!=A04_WIFI_LIFE_IO && s->state!=A04_WIFI_LIFE_FAILED))return -1;
 s->state=A04_WIFI_LIFE_STOPPING;s->transport.ready=0;
 if(s->ops.mask_and_drain_irq(s->ops.context) || s->ops.drain_io(s->ops.context) ||
    s->ops.unmap_host(s->ops.context) || s->ops.release_lease(s->ops.context)){
  fault(s);s->state=A04_WIFI_LIFE_STOP_FAILED;return -2;
 }
 a04_wifi_revoke(s->owner);s->state=A04_WIFI_LIFE_STOPPED;return 0;
}
