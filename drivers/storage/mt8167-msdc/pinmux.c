/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "pinmux.h"
/* MT8167 pinctrl: three mode bits per pin, five pins per 0x10-byte port,
 * mode registers start at 0x300. GPIO110..120 are MSDC0 in the board DTS. */
static const struct { unsigned offset; uint32_t mask,value; } fields[]={
    {0x460u,0x7fffu,0x1249u}, {0x470u,0x7fffu,0x1249u}, {0x480u,0x7u,0x1u}
};
enum a04_pinmux_result a04_msdc_pinmux_mode(struct a04_pinmux_state *s,
    const struct a04_pinmux_ops *o,uint32_t gen)
{
    unsigned i;uint32_t v;
    if(!s||!o||!o->held||!o->quiescent_and_powered||!o->read32||!o->update_masked||!gen)
        return A04_PINMUX_INVALID;
    if(s->poisoned)return A04_PINMUX_POISONED;
    if(o->held(o->context,gen)!=1)return A04_PINMUX_UNOWNED;
    if(o->quiescent_and_powered(o->context,gen)!=1)return A04_PINMUX_UNSAFE;
    for(i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
        if(o->held(o->context,gen)!=1) { s->poisoned=1;return A04_PINMUX_UNOWNED; }
        if(o->update_masked(o->context,fields[i].offset,fields[i].mask,fields[i].value)!=0)
            {s->poisoned=1;return A04_PINMUX_IO;}
        if(o->held(o->context,gen)!=1) { s->poisoned=1;return A04_PINMUX_UNOWNED; }
        if(o->read32(o->context,fields[i].offset,&v)!=0) {s->poisoned=1;return A04_PINMUX_IO;}
        if((v&fields[i].mask)!=fields[i].value) {s->poisoned=1;return A04_PINMUX_MISMATCH;}
    }
    return A04_PINMUX_OK;
}
/* Board DTS explicit UHS: CMD/DAT pull-up R1R0=01, CLK pull-down 10,
 * DATA/CMD drive 6 mA, CLK drive 8 mA, CMD/DAT input-enable. 114/RST is
 * left untouched: its resistance and drive are not specified in the DTS. */
static enum a04_pinmux_result uhs_check(struct a04_pinmux_state *s,
    const struct a04_pinmux_ops *o,uint32_t gen,unsigned off,uint32_t mask,uint32_t expected)
{
    uint32_t v;
    if(o->held(o->context,gen)!=1){s->poisoned=1;return A04_PINMUX_UNOWNED;}
    if(o->read32(o->context,off,&v)!=0){s->poisoned=1;return A04_PINMUX_IO;}
    if((v&mask)!=expected){s->poisoned=1;return A04_PINMUX_MISMATCH;}
    return A04_PINMUX_OK;
}
static enum a04_pinmux_result alias(struct a04_pinmux_state *s,
    const struct a04_pinmux_ops *o,uint32_t gen,unsigned base,uint32_t mask,int set)
{
    if(o->held(o->context,gen)!=1){s->poisoned=1;return A04_PINMUX_UNOWNED;}
    if(o->write_alias(o->context,base,mask,set)!=0){s->poisoned=1;return A04_PINMUX_IO;}
    return A04_PINMUX_OK;
}
static enum a04_pinmux_result update(struct a04_pinmux_state *s,
    const struct a04_pinmux_ops *o,uint32_t gen,unsigned off,uint32_t mask,uint32_t value)
{
    if(o->held(o->context,gen)!=1){s->poisoned=1;return A04_PINMUX_UNOWNED;}
    if(o->update_masked(o->context,off,mask,value)!=0){s->poisoned=1;return A04_PINMUX_IO;}
    return uhs_check(s,o,gen,off,mask,value);
}
enum a04_pinmux_result a04_msdc_pinmux_uhs(struct a04_pinmux_state *s,
    const struct a04_pinmux_ops *o,uint32_t gen)
{
    static const unsigned pull[]={0xe00u,0xe10u};unsigned i;enum a04_pinmux_result r;
    if(!s||!o||!o->held||!o->quiescent_and_powered||!o->read32||!o->update_masked||!o->write_alias||!gen)
        return A04_PINMUX_INVALID;
    if(s->poisoned)return A04_PINMUX_POISONED;
    if(o->held(o->context,gen)!=1)return A04_PINMUX_UNOWNED;
    if(o->quiescent_and_powered(o->context,gen)!=1)return A04_PINMUX_UNSAFE;
    for(i=0;i<2;++i) {
        r=alias(s,o,gen,pull[i],0x6666u,0);if(r!=A04_PINMUX_OK)return r;
        r=alias(s,o,gen,pull[i],0x1111u,1);if(r!=A04_PINMUX_OK)return r;
        r=uhs_check(s,o,gen,pull[i],0x7777u,0x1111u);if(r!=A04_PINMUX_OK)return r;
    }
    r=alias(s,o,gen,0xe20u,0x16u,0);if(r!=A04_PINMUX_OK)return r;
    r=alias(s,o,gen,0xe20u,0x61u,1);if(r!=A04_PINMUX_OK)return r;
    r=uhs_check(s,o,gen,0xe20u,0x77u,0x61u);if(r!=A04_PINMUX_OK)return r;
    r=update(s,o,gen,0xd60u,0x7700u,0x2300u);if(r!=A04_PINMUX_OK)return r;
    r=update(s,o,gen,0xd70u,0x7u,0x2u);if(r!=A04_PINMUX_OK)return r;
    r=alias(s,o,gen,0x920u,0x3feu,1);if(r!=A04_PINMUX_OK)return r;
    return uhs_check(s,o,gen,0x920u,0x3feu,0x3feu);
}
