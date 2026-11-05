/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "emmc_prepare.h"
static int overlap(const void *a,uintptr_t an,const void *b,uintptr_t bn)
{ uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y ? y-x<an : x-y<bn; }
static enum a04_emmc_prepare_result poison(struct a04_emmc_prepare_state *s,enum a04_emmc_prepare_result r)
{ s->board.poisoned=1;return r; }
enum a04_emmc_prepare_result a04_emmc_prepare(struct a04_emmc_prepare_state *s,
    const struct a04_emmc_prepare_ops *o,unsigned selector,unsigned limit,uint8_t scratch[512],
    struct a04_resume_observation *out)
{
    struct a04_vemc_result rail;struct a04_resume_observation obs;uint32_t gen;void *ctx;
    if(!s||!o||!o->board||!o->vemc||!o->pins||!o->cold||!o->host||(!o->host_prepare&&!o->reset_and_prepare_host)||
       !scratch||!out||!limit||selector>1)return A04_PREPARE_INVALID;
    if(!o->vemc->held_and_quiescent||!o->vemc->read32||!o->vemc->write32||!o->vemc->wait_us||
       !o->vemc->now||!o->vemc->timeout_ticks||!o->vemc->ramp_ticks||
       o->vemc->ramp_ticks>=o->vemc->timeout_ticks||!o->pins->held||!o->pins->quiescent_and_powered||
       !o->pins->read32||!o->pins->update_masked||!o->cold->prepare_valid||!o->cold->now||
       !o->cold->delay_us||!o->cold->ticks||!o->cold->rca||!o->host->read32||!o->host->read8||!o->host->write32)
        return A04_PREPARE_INVALID;
    if(overlap(scratch,512u,out,sizeof(*out)) || overlap(scratch,512u,s,sizeof(*s)) ||
       overlap(out,sizeof(*out),s,sizeof(*s)) || overlap(scratch,512u,o,sizeof(*o)) ||
       overlap(out,sizeof(*out),o,sizeof(*o)) || overlap(scratch,512u,o->board,sizeof(*o->board)) ||
       overlap(scratch,512u,o->vemc,sizeof(*o->vemc)) || overlap(scratch,512u,o->pins,sizeof(*o->pins)) ||
       overlap(scratch,512u,o->cold,sizeof(*o->cold)) || overlap(scratch,512u,o->host,sizeof(*o->host)) ||
       overlap(out,sizeof(*out),o->board,sizeof(*o->board)) || overlap(out,sizeof(*out),o->vemc,sizeof(*o->vemc)) ||
       overlap(out,sizeof(*out),o->pins,sizeof(*o->pins)) || overlap(out,sizeof(*out),o->cold,sizeof(*o->cold)) ||
       overlap(out,sizeof(*out),o->host,sizeof(*o->host))) return A04_PREPARE_INVALID;
    if(o->host_prepare && (o->host_prepare->io!=o->host || o->host_prepare->context!=o->board->context ||
       !o->host_prepare->owned_and_quiescent || !o->host_prepare->gate_clock || !o->host_prepare->verify_source || !o->host_prepare->now ||
       !o->host_prepare->timeout_ticks || !o->host_prepare->source_hz_max ||
       o->host_prepare->source_hz_max > 4258800000u ||
       overlap(scratch,512u,o->host_prepare,sizeof(*o->host_prepare)) ||
       overlap(out,sizeof(*out),o->host_prepare,sizeof(*o->host_prepare)) ||
       overlap(s,sizeof(*s),o->host_prepare,sizeof(*o->host_prepare))))return A04_PREPARE_INVALID;
    gen=o->cold->generation;ctx=o->board->context;
    if(!ctx||!gen||o->vemc->context!=ctx||o->pins->context!=ctx||o->cold->context!=ctx||o->host->context!=ctx)
        return A04_PREPARE_INVALID;
    if(a04_board_begin(&s->board,o->board,gen)!=A04_BOARD_OK)return A04_PREPARE_CLAIM;
    if(a04_vemc_restore(&s->vemc,o->vemc,gen,selector,limit,&rail)!=A04_VEMC_OK)
        return poison(s,A04_PREPARE_RAIL);
    if(a04_msdc_pinmux_mode(&s->pins,o->pins,gen)!=A04_PINMUX_OK)
        return poison(s,A04_PREPARE_PIN);
    if(a04_board_finish_clocks(&s->board,o->board,gen)!=A04_BOARD_OK)
        return poison(s,A04_PREPARE_CLOCK);
    if(o->board->owned(ctx,gen)!=1)return poison(s,A04_PREPARE_HOST);
    if(o->host_prepare && a04_board_verify_emmc_mux(&s->board,o->board,gen)!=A04_BOARD_OK)
        return poison(s,A04_PREPARE_CLOCK);
    if(o->host_prepare ? a04_host_prepare(&s->host,o->host_prepare,gen,limit)!=A04_HOST_OK :
       o->reset_and_prepare_host(ctx,gen)!=1)return poison(s,A04_PREPARE_HOST);
    if(o->board->owned(ctx,gen)!=1)return poison(s,A04_PREPARE_HOST);
    if(a04_emmc_cold_init(o->host,o->cold,limit,scratch,&obs)!=A04_COLD_OK)
        return poison(s,A04_PREPARE_CARD);
    if(o->board->owned(ctx,gen)!=1)return poison(s,A04_PREPARE_CARD);
    obs.power_owned=0;obs.clock_stable=0;*out=obs;
    return A04_PREPARE_OBSERVED;
}
