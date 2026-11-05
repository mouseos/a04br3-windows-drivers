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

/* Linux a04br3: VBREG=0x29 (4.32 V), ICHRG=0x0a (1.3 A),
 * ITERM=1 (100 mA), TMR=1 (6 h). Restrict USB to 100/500 mA;
 * the caller must independently establish its permitted input limit. */
int a04_bq_restore_linux(const struct a04_bq_recovery_proof *p,const struct a04_bq_recovery_io *io,
                         uint64_t deadline,unsigned usb_code,const struct a04_bq_recovery_result *prior,
                         struct a04_bq_restore_result *out)
{
    static const uint8_t regs[4]={7,5,3,2};
    uint8_t values[4]={0x28,0x51,0xa4,0}, status,bat,temp,v;
    uint64_t start,previous,now;unsigned i;
    if(!out)return -1;
    memset(out,0,sizeof(*out));
    if(!p || !p->exclusive_bus || !p->firmware_stopped || !p->worker_armed ||
       !p->battery_can_supply_system || !p->board_profile_and_usb_limit_valid ||
       !io || !io->now_ns || !io->read || !io->write || !deadline ||
       (usb_code!=0 && usb_code!=2) || !prior || prior->stage!=12 ||
       prior->hiz_outcome!=A04_BQ_HIZ_CLEAR_ACK)return -1;
    values[3]=(uint8_t)(0x0c | (usb_code<<4));
    start=io->now_ns(io->context);previous=start;if(start>=deadline)return -1;
#define CHECK_RESTORE_TIME() do { now=io->now_ns(io->context); if(now<previous || now>=deadline || now-start>=1000000000ULL)return -1; previous=now; } while (0)
    /* Do not automatically clear fresh safety, source, battery or TS faults. */
    CHECK_RESTORE_TIME();if(io->read(io->context,0,&status))return -1;
    CHECK_RESTORE_TIME();if(io->read(io->context,1,&bat))return -1;
    CHECK_RESTORE_TIME();if(io->read(io->context,7,&temp))return -1;
    CHECK_RESTORE_TIME();
    if ((status&7u) || (bat&15u) || ((bat&0xc0u) && (bat&0x30u)) ||
        (temp&6u) || !(temp&8u))return -1;
    for(i=0;i<4;i++) {
        CHECK_RESTORE_TIME();out->stage=i+1;
        if(io->write(io->context,regs[i],values[i]))return -1;
    }
    for(i=0;i<4;i++) {
        CHECK_RESTORE_TIME();
        if(io->read(io->context,regs[i],&v))return -1;
        out->readback[i]=v;
        if ((v & (i==3 ? 0x7f : i==2 ? 0xfe : i==0 ? 0xe9 : 0xff)) !=
            (values[i] & (i==3 ? 0x7f : i==2 ? 0xfe : i==0 ? 0xe9 : 0xff)))return -1;
    }
    CHECK_RESTORE_TIME();out->stage=5;return 0;
#undef CHECK_RESTORE_TIME
}
