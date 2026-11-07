/* SPDX-License-Identifier: MIT */
#include "cold_profile.h"
#include <string.h>
/* Board Linux bq24160_charge_init: 6h timer, 1.3A charge/100mA termination,
 * 4.32V regulation. USB stays 100mA: no host/charger classification inferred. */
int a04_bq_cold_profile(struct a04_bq_owner *o,const struct a04_bq_recovery_io *io,
                       struct a04_bq_cold_profile_result *r)
{
    static const uint8_t reg[4]={7,5,3,2},val[4]={0x28,0x51,0xa4,0x0c};
    uint64_t start,previous,now;unsigned i;uint8_t status,source;
    if(!r)return -1;
    memset(r,0,sizeof(*r));
    if(!o || !io || !io->now_ns || !io->read || !io->write || o->stopped ||
       o->handoff_pending!=2 || o->attempts || o->kicks || o->firmware_kicks || o->cold_profile_attempted)return -1;
    start=previous=io->now_ns(io->context);r->before.at=start;
#define TIME() do {now=io->now_ns(io->context);if(now<previous || now>=o->handoff_deadline || now-start>=1000000000ULL)goto fail;previous=now;}while(0)
    TIME();
    /* First read can start the hardware watchdog. Consume before that read;
     * success must not permit a second configuration before the first kick. */
    o->cold_profile_attempted=1;
    for(i=0;i<8;i++) {
        TIME();if(io->read(io->context,(uint8_t)i,&r->before.raw[i]))goto fail;
        r->before.valid|=1u<<i;r->stage=i+1;
    }
    TIME();status=r->before.raw[0];source=r->before.raw[1];
    /* Never erase or recover a reported fault, BATOVP, bad TS or missing
     * input. Clearing BATOVP requires a separately qualified HiZ/CD path. */
    if((r->before.raw[4]&0xf8u)!=0x40u || (status&15u) || (source&15u) ||
       ((source&0xc0u) && (source&0x30u)) || (r->before.raw[7]&6u) ||
       (r->before.raw[2]&1u) || (r->before.raw[6]&0x3fu))goto fail;
    for(i=0;i<4;i++) {
        TIME();r->stage=9+i;r->write_attempted=1;
        if(io->write(io->context,reg[i],val[i]))goto fail;
    }
    for(i=0;i<4;i++) {
        uint8_t mask=i==0?0xe9u:i==2?0xfeu:i==3?0x7fu:0xffu;
        TIME();r->stage=13+i;
        if(io->read(io->context,reg[i],&r->readback[i]) ||
           (r->readback[i]&mask)!=(val[i]&mask))goto fail;
    }
    TIME();r->stage=17;return 0;
fail:
    /* Preserve the complete/partial pre-write observation on failure. No
     * retry/rollback: any failed write may already have reached the PMIC. */
    o->latest=r->before;o->first=r->before;
    if(r->before.valid&1u)o->fault_history|=1u<<(r->before.raw[0]&7u);
    o->stopped=1;return -1;
#undef TIME
}
