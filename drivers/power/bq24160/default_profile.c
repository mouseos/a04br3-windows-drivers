/* SPDX-License-Identifier: MIT */
#include "default_profile.h"
#include <string.h>
/* TI SLUSAO0H 8.6.1--8.6.8. Exclude 00 TMR_RST/read-only STAT/FAULT,
 * 01 input/battery status, 02 RESET always-1 read value, 03 self-clearing
 * D+/D-_EN, 04 revision, 06 live DPM/MINSYS, 07 reserved/TS_FAULT.
 * 07 reset row and 2XTMR_EN default prose disagree: preserve bit7 raw rather
 * than guessing. TMR[6:5]=00, TS_EN=1, LOW_CHG=0 remain mandatory; timing
 * therefore may be extended by 2x and is not reported as exactly 27 minutes. */
static const uint8_t mask[8]={0x08,0x09,0x7f,0xfe,0xf8,0xff,0x3f,0x69};
static const uint8_t expected[8]={0,0,0x0c,0x14,0x40,0x32,0,0x08};
static int observe(const struct a04_bq_default_io *io,uint64_t deadline,
                    struct a04_bq_default_result *r,int reset)
{
 uint64_t start,previous,now;unsigned i;
 if(!r)return -1;
 memset(r,0,sizeof(*r));
 if(!io||!io->now_ns||!io->read||(reset&&!io->reset)||!deadline)return -1;
 start=previous=io->now_ns(io->context);r->snapshot.at=start;
#define TIME() do{now=io->now_ns(io->context);if(now<previous||now>=deadline||now-start>=1000000000ULL){r->reason=A04_DEFAULT_TIME;return -1;}previous=now;}while(0)
 TIME();
 if(reset){r->stage=1;if(io->reset(io->context)){r->reason=A04_DEFAULT_IO;return -1;}r->reset_acked=1;TIME();}
 for(i=0;i<8;++i){
  TIME();r->stage=i+2;
  /* A failed transfer may still start watchdog; do not retry or fabricate data. */
  r->last_access=now;
  if(io->read(io->context,(uint8_t)i,&r->snapshot.raw[i])){r->reason=A04_DEFAULT_IO;return -1;}
  r->snapshot.valid|=1u<<i;
 }
 TIME();
 for(i=0;i<8;++i)if((r->snapshot.raw[i]&mask[i])!=expected[i])r->mismatch|=1u<<i;
 if(r->snapshot.raw[2]&0x70u){r->reason=A04_DEFAULT_BUDGET;return -1;}
 if(r->mismatch){r->reason=A04_DEFAULT_CONFIG;return -1;}
 if((r->snapshot.raw[0]&7u)||(r->snapshot.raw[1]&6u)||(r->snapshot.raw[7]&6u)||
    ((r->snapshot.raw[0]>>4)&7u)>=6u){r->reason=A04_DEFAULT_FAULT;return -1;}
 r->matched=1;r->reason=A04_DEFAULT_OK;r->stage=10;return 0;
#undef TIME
}
int a04_bq_read_default(const struct a04_bq_default_io *io,uint64_t d,struct a04_bq_default_result *r)
{return observe(io,d,r,0);}
int a04_bq_reset_read_default(const struct a04_bq_default_io *io,uint64_t d,struct a04_bq_default_result *r)
{return observe(io,d,r,1);}
