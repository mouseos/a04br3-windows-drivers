/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_TOPRGU_RX_OBSERVE_H
#define A04_TOPRGU_RX_OBSERVE_H
#include <stdint.h>
/* Private context only. No MMIO, allocation, raw RBR bytes or shared ABI here. */
#define A04_RX_MAX_RECORDS 24u
#define A04_RX_IDLE_RECORDS 4u
#define A04_RX_EVENT_RECORDS 20u
#define A04_RX_WORDS 22u
#define A04_RX_LINE_BYTES (7u+9u*A04_RX_WORDS)
#define A04_RX_CHANGE_INTERVAL UINT64_C(100000000) /* 10 seconds */
#define A04_RX_HEARTBEAT_INTERVAL UINT64_C(600000000) /* 60 seconds */
struct a04_rx_observe {
 uint32_t d0,polls,lcr_reads,lsr_reads,dr,overrun,bytes,parser_calls,advance,mismatch,hits,deferred,saturated,dlab,lcr_changes;
 uint32_t lcr,lsr,lsr_valid,matched,records,idle_records,event_records;
 uint32_t saved_lcr,saved_lsr,saved_matched,saved_bytes,saved_d0,saved_saturated,saved_dlab,saved_lcr_changes,saved_overrun;
 uint64_t queued_at;
 uint32_t length,position;
 char line[A04_RX_LINE_BYTES];
};
static void a04_rx_inc(struct a04_rx_observe *s,uint32_t *v,uint32_t bit)
{if(*v==UINT32_MAX)s->saturated|=bit;else ++*v;}
static void a04_rx_d0(struct a04_rx_observe *s)
{
 /* Keep the lifetime budget/counters and any partially transmitted record.
  * An old-D0 queued record remains explicitly tagged with that old D0. */
 a04_rx_inc(s,&s->d0,1u);s->lsr_valid=0;
}
static void a04_rx_begin(struct a04_rx_observe *s)
{a04_rx_inc(s,&s->polls,2u);s->lsr_valid=0;}
static void a04_rx_lcr(struct a04_rx_observe *s,uint32_t v)
{
 if(s->lcr_reads && s->lcr!=(v&255u))a04_rx_inc(s,&s->lcr_changes,8192u);
 if(v&128u)a04_rx_inc(s,&s->dlab,4096u);
 a04_rx_inc(s,&s->lcr_reads,4u);s->lcr=v&255u;s->lsr_valid=0;
}
static void a04_rx_lsr(struct a04_rx_observe *s,uint32_t v)
{
 a04_rx_inc(s,&s->lsr_reads,8u);s->lsr=v&255u;s->lsr_valid=1;
 if(v&1u)a04_rx_inc(s,&s->dr,16u);
 if(v&2u)a04_rx_inc(s,&s->overrun,32u);
}
static void a04_rx_consumed(struct a04_rx_observe *s,uint32_t before,uint32_t after,int hit)
{
 a04_rx_inc(s,&s->bytes,64u);a04_rx_inc(s,&s->parser_calls,128u);s->matched=after;
 if(hit)a04_rx_inc(s,&s->hits,256u);
 else if(before<9u && after==before+1u)a04_rx_inc(s,&s->advance,512u);
 else a04_rx_inc(s,&s->mismatch,1024u);
}
static int a04_rx_can_tx(const struct a04_rx_observe *s)
{return s->lsr_valid && !(s->lcr&128u) && (s->lsr&32u);}
static void a04_rx_queue(struct a04_rx_observe *s,uint64_t now,uint32_t matched,int screen_idle)
{
 uint32_t words[A04_RX_WORDS],i,j,at=7u,key;int changed;uint64_t elapsed;
 static const char hex[]="0123456789ABCDEF";
 s->matched=matched;
 if(s->length || s->records>=A04_RX_MAX_RECORDS)return;
 key=(s->lsr_valid?256u:0u)|(s->lsr_valid?(s->lsr&31u):0u);
 elapsed=now-s->queued_at;
 changed=s->records && (s->saved_lcr!=s->lcr || s->saved_lsr!=key ||
    s->saved_matched!=matched || s->saved_bytes!=s->bytes || s->saved_d0!=s->d0 || s->saved_saturated!=s->saturated || s->saved_dlab!=s->dlab ||
    s->saved_lcr_changes!=s->lcr_changes || s->saved_overrun!=s->overrun);
 if(s->records && elapsed<A04_RX_CHANGE_INTERVAL)return;
 /* Reserve twenty records for late RX/state changes. Idle heartbeats cannot
  * spend the entire lifetime budget before a host eventually sends input. */
 if(changed){if(s->event_records>=A04_RX_EVENT_RECORDS)return;}
 else if(s->idle_records>=A04_RX_IDLE_RECORDS || (s->records && elapsed<A04_RX_HEARTBEAT_INTERVAL))return;
 if(!screen_idle){a04_rx_inc(s,&s->deferred,2048u);return;}
 if(changed)s->event_records++;else s->idle_records++;
 s->records++;s->queued_at=now;s->saved_lcr=s->lcr;s->saved_lsr=key;s->saved_matched=matched;
 s->saved_bytes=s->bytes;s->saved_d0=s->d0;s->saved_saturated=s->saturated;
 s->saved_dlab=s->dlab;s->saved_lcr_changes=s->lcr_changes;s->saved_overrun=s->overrun;
 words[0]=s->records;words[1]=s->d0;words[2]=(uint32_t)(now/10000u);
 words[3]=s->lsr_valid;words[4]=s->lcr;words[5]=s->lsr;
 words[6]=s->polls;words[7]=s->lcr_reads;words[8]=s->lsr_reads;
 words[9]=s->dr;words[10]=s->overrun;words[11]=s->bytes;words[12]=s->parser_calls;
 words[13]=s->advance;words[14]=s->mismatch;words[15]=s->hits;
 words[16]=s->matched;words[17]=s->saturated;words[18]=s->deferred;words[19]=(A04_RX_MAX_RECORDS<<16)|(s->idle_records<<8)|s->event_records;
 s->line[0]='A';s->line[1]='0';s->line[2]='4';s->line[3]='R';s->line[4]='X';s->line[5]='1';s->line[6]=' ';
 for(i=0;i<A04_RX_WORDS;i++){
  for(j=0;j<8;j++)s->line[at++]=hex[(words[i]>>(28u-4u*j))&15u];
  s->line[at++]=i+1u==A04_RX_WORDS?'\n':' ';
 }
 s->position=0;s->length=at;
}
static void a04_rx_sent(struct a04_rx_observe *s)
{if(s->length && ++s->position==s->length)s->position=s->length=0;}
#endif
