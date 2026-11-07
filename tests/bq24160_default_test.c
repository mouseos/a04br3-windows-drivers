/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "default_profile.h"
struct fake {uint64_t now;unsigned reads,resets;int fail_reset,fail_read;uint8_t raw[8];};
static uint64_t clock_ns(void*p){return ((struct fake*)p)->now;}
static int reset(void*p){struct fake*f=p;++f->resets;return f->fail_reset;}
static int read_reg(void*p,uint8_t n,uint8_t*v){struct fake*f=p;++f->reads;if(f->fail_read==(int)n+1)return -1;*v=f->raw[n];return 0;}
static void init(struct fake*f){static const uint8_t d[8]={0x40,0xc0,0x8c,0x14,0x45,0x32,0xc0,0x98};memset(f,0,sizeof(*f));memcpy(f->raw,d,8);}
int main(void){struct fake f;struct a04_bq_default_result r;struct a04_bq_default_io io={&f,clock_ns,read_reg,reset};unsigned n;
 init(&f);assert(!a04_bq_reset_read_default(&io,1000000000ULL,&r));assert(r.matched&&r.reset_acked&&r.snapshot.valid==255&&f.resets==1&&f.reads==8);
 /* RESET is always1 on read; TMR_RST is not a state test. Live DPM, revision,
  * self-clearing detection, reserved bits and conflicting 2x default differ. */
 init(&f);f.raw[2]&=0x7f;f.raw[3]|=1;f.raw[4]=0x43;f.raw[6]=0;f.raw[7]=8;
 assert(!a04_bq_read_default(&io,1000000000ULL,&r));assert(!r.reset_acked&&!f.resets);
 /* Mutating any mandatory configuration bit rejects real readback. */
 static const uint8_t masks[8]={8,9,0x7f,0xfe,0xf8,0xff,0x3f,0x69};
 for(n=0;n<8;++n)for(unsigned bit=1;bit<256;bit<<=1)if(masks[n]&bit){init(&f);f.raw[n]^=bit;assert(a04_bq_reset_read_default(&io,1000000000ULL,&r));assert(!r.matched&&r.reset_acked);}
 /* No budget upgrades accepted, including the chip's 1.5A detector outcome. */
 for(n=1;n<8;++n){init(&f);f.raw[2]=(uint8_t)(0x8c|(n<<4));assert(a04_bq_read_default(&io,1000000000ULL,&r));assert(r.reason==A04_DEFAULT_BUDGET);}
 for(n=1;n<9;++n){init(&f);f.fail_read=(int)n;assert(a04_bq_reset_read_default(&io,1000000000ULL,&r));assert(!r.matched&&r.snapshot.valid==((1u<<(n-1))-1)&&f.reads==n&&f.resets==1);}
 init(&f);f.fail_reset=1;assert(a04_bq_reset_read_default(&io,1000000000ULL,&r));assert(!r.reset_acked&&!f.reads);
 init(&f);f.now=1000000000ULL;assert(a04_bq_reset_read_default(&io,1000000000ULL,&r));assert(!f.reads&&!f.resets&&r.reason==A04_DEFAULT_TIME);
 for(n=1;n<8;++n){init(&f);f.raw[0]|=(uint8_t)n;assert(a04_bq_read_default(&io,1000000000ULL,&r));assert(r.reason==A04_DEFAULT_FAULT&&r.snapshot.raw[0]==(0x40|n));}
 init(&f);f.raw[2]|=2;assert(a04_bq_read_default(&io,1000000000ULL,&r)); /* safety timeout CE1: do not auto-reenable */
 return 0;}
