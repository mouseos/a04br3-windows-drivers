/* SPDX-License-Identifier: MIT */
#include "recovery.h"
#include <string.h>
/* TI SLUSAO0H 8.6.3/8.6.4: HZ isolates input and RESET reads as 1.
 * Never echo bit 7 of register 02. The platform must prove battery backup. */
int a04_bq_recover_batovp(const struct a04_bq_recovery_proof *p,const struct a04_bq_recovery_io *io,
                          uint64_t deadline,struct a04_bq_recovery_result *r)
{
    uint64_t start,previous,now;
    uint8_t control,value;
    unsigned i;
    if (!r) return -1;
    memset(r,0,sizeof(*r));
    if (!p || !p->exclusive_bus || !p->firmware_stopped || !p->worker_armed ||
        !p->battery_can_supply_system || !p->board_profile_and_usb_limit_valid ||
        !io || !io->read || !io->write || !io->now_ns || !deadline) return -1;
    start=io->now_ns(io->context);previous=start;
    if(start>=deadline)return -1;
#define CHECK_TIME() do { now=io->now_ns(io->context); if(now<previous || now>=deadline || now-start>=1000000000ULL) return -1; previous=now; } while (0)
    for(i=0;i<8;i++) {
        CHECK_TIME();
        if(io->read(io->context,(uint8_t)i,&r->before[i]))return -1;
        r->stage=i+1;
    }
    CHECK_TIME();
    /* Only battery OVP with no thermal, supply, or other charger fault. */
    if ((r->before[4]&0xf8u)!=0x40u || ((r->before[1]>>1)&3u)!=1u ||
        ((r->before[0]&7u)!=0u && (r->before[0]&7u)!=7u) ||
        (r->before[7]&6u) || (r->before[1]&9u) ||
        ((r->before[1]&0xc0u) && (r->before[1]&0x30u)) || (r->before[2]&1u)) return -1;
    control=(uint8_t)(r->before[2]&0x7eu); /* exclude RESET and initial HZ */
    r->write_attempted=1;r->stage=9;r->hiz_outcome=A04_BQ_HIZ_ASSERT_UNCERTAIN;
    if(io->write(io->context,2,(uint8_t)(control|1u))) return -1;
    r->hiz_outcome=A04_BQ_HIZ_ASSERT_ACK;
    CHECK_TIME();r->stage=10;r->hiz_outcome=A04_BQ_HIZ_CLEAR_UNCERTAIN;
    if(io->write(io->context,2,control))return -1;
    r->hiz_outcome=A04_BQ_HIZ_CLEAR_ACK;
    CHECK_TIME();r->stage=11;
    if(io->read(io->context,1,&value))return -1;
    r->after_battery=value;
    CHECK_TIME();
    if (((value>>1)&3u)!=0u || (value&9u) ||
        ((value&0xc0u) && (value&0x30u)))return -1;
    r->stage=12;return 0;
#undef CHECK_TIME
}
