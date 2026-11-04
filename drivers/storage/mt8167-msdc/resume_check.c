/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "resume_check.h"
static int width_ok(uint8_t v) { return v==1 || v==4 || v==8; }
static int owner_ok(const struct a04_resume_observation *o)
{
    /* Exact values avoid treating corrupted bitfields as positive evidence. */
    return o->power_owned==1 && o->clock_stable==1 && o->pio_mode==1 &&
           o->dma_idle==1 && o->events_masked==1;
}
static int card_ok(const struct a04_resume_observation *o)
{
    return o->rca && o->sectors && !(o->card_status & 0xffffe0c0u) &&
           (o->card_status & (1u<<8)) && ((o->card_status >> 9)&15u)==4;
}
static int mode_ok(const struct a04_resume_observation *o)
{
    return width_ok(o->host_width) && o->host_width==o->card_width &&
           o->partition==0 && o->cache_ctrl==0;
}
enum a04_resume_check a04_resume_validate(const struct a04_resume_observation *b,
                                           const struct a04_resume_observation *c)
{
    unsigned i; uint8_t any=0, all=0xffu;
    if(!b || !c) return A04_RESUME_INVALID;
    if(!b->generation || c->generation<=b->generation) return A04_RESUME_GENERATION;
    if(!owner_ok(b) || !owner_ok(c)) return A04_RESUME_OWNER;
    if(!card_ok(b) || !card_ok(c)) return A04_RESUME_CARD_STATE;
    if(!mode_ok(b) || !mode_ok(c)) return A04_RESUME_MODE;
    for(i=0;i<16;++i) { any|=b->cid[i]; all&=b->cid[i]; if(b->cid[i]!=c->cid[i]) return A04_RESUME_IDENTITY; }
    if(!any || all==0xffu || b->sectors!=c->sectors || b->rca!=c->rca) return A04_RESUME_IDENTITY;
    if(b->host_width!=c->host_width) return A04_RESUME_MODE;
    return A04_RESUME_VALID;
}
