/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "resource_lease.h"
#define ALL_RESOURCES ((1u << A04_LEASE_COUNT)-1u)
int a04_resource_lease_acquire(struct a04_resource_lease *l,const struct a04_lease_ops *ops,uint32_t *generation)
{
 unsigned i;
 if(!l||!ops||!generation||!ops->acquire||!ops->release||!ops->drain_irq||!ops->drain_io||
    (l->state!=A04_LEASE_OFF && l->state!=A04_LEASE_STOPPED)||l->held||l->generation==UINT32_MAX)return -1;
 l->owner=0;l->ops=*ops;l->state=A04_LEASE_ACQUIRING;++l->generation;
 for(i=0;i<A04_LEASE_COUNT;i++){
  /* Include the attempted resource: a failing callback can have taken effect. */
  l->held|=1u<<i;
  if(l->ops.acquire(l->ops.context,(enum a04_lease_resource)i)){l->state=A04_LEASE_FAILED;return -1;}
 }
 l->state=A04_LEASE_ACTIVE;*generation=l->generation;return 0;
}
int a04_resource_lease_live(const struct a04_resource_lease *l,uint32_t generation)
{ return l && l->state==A04_LEASE_ACTIVE && l->held==ALL_RESOURCES && l->generation==generation && generation!=0; }
int a04_resource_lease_bind_owner(struct a04_resource_lease *l,struct a04_wifi_owner *o)
{
 if(!l||!o||l->owner||o->active||o->poisoned||!a04_resource_lease_live(l,l->generation))return -1;
 l->owner=o;o->generation=l->generation;o->host=o->gpio_clock=o->combo=o->firmware=o->irq=1;
 return a04_wifi_admit(o);
}
int a04_resource_lease_same_owner(void *context,const struct a04_wifi_owner *o,unsigned generation)
{
 struct a04_resource_lease *l=context;
 return l && l->owner==o && o && o->active && !o->poisoned && o->generation==generation && a04_resource_lease_live(l,generation);
}
int a04_resource_lease_stop(struct a04_resource_lease *l,uint32_t generation)
{
 unsigned i;
 if(!a04_resource_lease_live(l,generation) || (l->owner &&
    (!l->owner->active || l->owner->poisoned || l->owner->generation!=generation)))return -1;
 l->state=A04_LEASE_STOPPING;
 if(l->owner)a04_wifi_revoke(l->owner);
 if(l->ops.drain_irq(l->ops.context)||l->ops.drain_io(l->ops.context))goto failed;
 for(i=A04_LEASE_COUNT;i>0;i--){
  if(l->ops.release(l->ops.context,(enum a04_lease_resource)(i-1)))goto failed;
  l->held&=~(1u<<(i-1));
 }
 l->state=A04_LEASE_STOPPED;return 0;
failed:if(l->owner)l->owner->poisoned=1;l->state=A04_LEASE_FAILED;return -1;
}
