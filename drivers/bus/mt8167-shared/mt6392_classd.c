/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Hardware sequence reference: MT6392 Linux board kernel mt6392-codec.c,
 * USE_HW_TRIM_CLASS_D and mt6392_int_spk_turn_off. No gain/unmute policy is
 * inferred. Real register operations require the shared parent's proven bus. */
#include "mt6392_classd.h"
#define SPK0 0x0052u
#define SPK2 0x0056u
#define SPK7 0x0060u
#define SPK9 0x0064u
#define SPK11 0x0068u
#define SPK12 0x006au
#define CK_CLR 0x010cu
struct classd_update { uint16_t reg,mask,bits; };
static const struct classd_update start_ops[]={
    {SPK9,0x0f00,0},{CK_CLR,0x000e,0x000e},{SPK7,0xffff,0x48f4},
    {SPK11,0xffff,0x0055},{SPK9,0xf0ff,0x2018},{SPK2,0xffff,0x0414},
    {SPK0,0xffff,0x3409},{SPK0,0xffff,0x3401},{SPK9,0xf0ff,0x2000}
};
static const struct classd_update stop_ops[]={
    {SPK9,0x0f00,0},{SPK12,0xffff,0},{SPK0,0xffff,0x3400},{CK_CLR,0x000e,0}
};
static int supported(struct a04_mt6392_classd *c,const struct classd_update *ops,unsigned n)
{
    unsigned i;if(!c->supports)return 0;
    for(i=0;i<n;i++)if(c->supports(c->context,ops[i].reg,ops[i].mask,ops[i].bits))return 0;
    return 1;
}
static int fail(struct a04_mt6392_classd *c) { c->poisoned=1;return -1; }
static int upd(struct a04_mt6392_classd *c,uint16_t reg,uint16_t mask,uint16_t bits)
{ return c->update_bits(c->context,reg,mask,bits); }
int a04_mt6392_classd_start_muted(struct a04_mt6392_classd *c)
{
    uint16_t gain;
    if(!c || c->active || c->poisoned || !c->verify_start || !c->verify_quiescent ||
       !c->update_bits || !c->read16 || !c->delay_us || c->verify_start(c->context) ||
       !supported(c,start_ops,sizeof(start_ops)/sizeof(start_ops[0])) ||
       !supported(c,stop_ops,sizeof(stop_ops)/sizeof(stop_ops[0]))) return -1;
    /* Linux enumerates SPK_CON9 gain code 0 as MUTE; readback alone does not
     * establish board-level silence, hence the separate prerequisite. */
    if(upd(c,SPK9,0x0f00,0) || c->read16(c->context,SPK9,&gain) || (gain&0x0f00)) return fail(c);
    if(upd(c,CK_CLR,0x000e,0x000e) || upd(c,SPK7,0xffff,0x48f4) ||
       upd(c,SPK11,0xffff,0x0055) || upd(c,SPK9,0xf0ff,0x2018) ||
       upd(c,SPK2,0xffff,0x0414) || upd(c,SPK0,0xffff,0x3409) ||
       c->delay_us(c->context,20000) || upd(c,SPK0,0xffff,0x3401) ||
       upd(c,SPK9,0xf0ff,0x2000) || c->delay_us(c->context,2000)) return fail(c);
    if(c->read16(c->context,SPK9,&gain) || (gain&0x0f00)) return fail(c);
    c->active=1;return 0;
}
/* Linux MT6392 gain code 1 is +0 dB, the lowest non-mute code. No larger
 * gain or board route is inferred. The diagnostic callback must independently
 * establish the output route and power before any write. */
int a04_mt6392_classd_unmute_min(struct a04_mt6392_classd *c)
{
    uint16_t gain;
    if(!c || !c->active || c->poisoned || !c->verify_unmute ||
       !c->supports || !c->update_bits || !c->read16 ||
       c->verify_unmute(c->context) || c->supports(c->context,SPK9,0x0f00,0x0100))return -1;
    if(upd(c,SPK9,0x0f00,0x0100) || c->read16(c->context,SPK9,&gain) ||
       (gain&0x0f00)!=0x0100)return fail(c);
    return 0;
}
/* Mute may be required while DMA drain is still unknown. This operation
 * never disables the amplifier, a rail, or either clock lease. */
int a04_mt6392_classd_mute(struct a04_mt6392_classd *c)
{
    uint16_t gain;
    if(!c || !c->active || c->poisoned || !c->supports || !c->update_bits ||
       !c->read16 || c->supports(c->context,SPK9,0x0f00,0)) return -1;
    if(upd(c,SPK9,0x0f00,0) || c->read16(c->context,SPK9,&gain) || (gain&0x0f00)) return fail(c);
    return 0;
}
int a04_mt6392_classd_stop(struct a04_mt6392_classd *c)
{
    uint16_t gain;
    if(!c || !c->active || c->poisoned || !c->verify_quiescent ||
       !c->update_bits || !c->read16 || c->verify_quiescent(c->context) ||
       !supported(c,stop_ops,sizeof(stop_ops)/sizeof(stop_ops[0]))) return -1;
    if(upd(c,SPK9,0x0f00,0) || c->read16(c->context,SPK9,&gain) || (gain&0x0f00) ||
       upd(c,SPK12,0xffff,0) || upd(c,SPK0,0xffff,0x3400) ||
       upd(c,CK_CLR,0x000e,0)) return fail(c);
    c->active=0;return 0;
}
