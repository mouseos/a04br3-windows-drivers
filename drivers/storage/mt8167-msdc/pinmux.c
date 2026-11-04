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
