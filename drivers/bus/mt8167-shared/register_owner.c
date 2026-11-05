/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "register_owner.h"
static int enter(struct a04_register_owner *o)
{ if(!o||!o->lock||!o->unlock||!o->read32||!o->write32)return -1;return o->lock(o->context); }
static int allowed_update(enum a04_shared_bank b,unsigned off,uint32_t mask,uint32_t bits)
{
    return (b==A04_SHARED_GPIO && off==0x3b0u && ((mask==(7u<<9)&&bits==(1u<<9))||(mask==(7u<<12)&&bits==(1u<<12)))) ||
           (b==A04_SHARED_GPIO && off==0x310u && mask==(7u<<9) && !bits) ||
           (b==A04_SHARED_INFRA && off==0x80u && mask==(1u<<1) && !bits);
}
int a04_register_owner_start(struct a04_register_owner *o)
{
    int result=-1;if(enter(o))return -1;
    if(!o->online && !o->poisoned){o->online=1;result=0;}
    o->unlock(o->context);return result;
}
int a04_register_touch_update(struct a04_register_owner *o,enum a04_shared_bank b,unsigned off,uint32_t mask,uint32_t bits)
{
    uint32_t v,readback;int result=-1;
    if(!allowed_update(b,off,mask,bits)||enter(o))return -1;
    if(!o->online||o->poisoned)goto out;
    if(o->read32(o->context,b,off,&v)||o->write32(o->context,b,off,(v&~mask)|bits)||
       o->read32(o->context,b,off,&readback)||(readback&mask)!=bits){o->poisoned=1;goto out;}
    result=0;
out:o->unlock(o->context);return result;
}
int a04_register_touch_write(struct a04_register_owner *o,enum a04_shared_bank b,unsigned off,uint32_t value)
{
    int result=-1;
    if(!((b==A04_SHARED_GPIO && off==0x08u && value==(1u<<8)) ||
         (b==A04_SHARED_GPIO && off==0x904u && value==((1u<<3)|(1u<<15))) ||
         (b==A04_SHARED_TOP && off==0x84u && value==(1u<<3)))||enter(o))return -1;
    if(!o->online||o->poisoned)goto out;
    if(o->write32(o->context,b,off,value)){o->poisoned=1;goto out;}
    result=0;
out:o->unlock(o->context);return result;
}
int a04_register_touch_input(struct a04_register_owner *o,int *down)
{
    uint32_t v;int result=-1;
    if(!down||enter(o))return -1;
    if(!o->online||o->poisoned)goto out;
    if(o->read32(o->context,A04_SHARED_GPIO,0x200u,&v)){o->poisoned=1;goto out;}
    *down=(v&(1u<<8))==0;result=0;
out:o->unlock(o->context);return result;
}
int a04_register_owner_stop(struct a04_register_owner *o)
{ if(enter(o))return -1;o->online=0;o->unlock(o->context);return 0; }

/* MT8167 CLK_TOP_AUDIO: status TOP1 0x24 bit 25; CLR 0x84 enables,
 * SET 0x54 disables. Only the original gate state is restored. */
#define A04_TOP_AUDIO_BIT (1u<<25)
int a04_audio_top_clock_acquire(struct a04_register_owner *o,struct a04_audio_top_clock *s)
{
    uint32_t v;
    if(!o || !s || !o->read32 || !o->write32 || s->held || s->poisoned) return -1;
    if(o->read32(o->context,A04_SHARED_TOP,0x24u,&v)) {s->poisoned=1;return -1;}
    s->held=1;s->restore_gated=(v&A04_TOP_AUDIO_BIT)!=0;
    if(s->restore_gated && (o->write32(o->context,A04_SHARED_TOP,0x84u,A04_TOP_AUDIO_BIT) ||
       o->read32(o->context,A04_SHARED_TOP,0x24u,&v) || (v&A04_TOP_AUDIO_BIT))) {
        s->poisoned=1;return -1;
    }
    return 0;
}
int a04_audio_top_clock_release(struct a04_register_owner *o,struct a04_audio_top_clock *s)
{
    uint32_t v;
    if(!o || !s || !o->read32 || !o->write32 || !s->held || s->poisoned) return -1;
    if(o->read32(o->context,A04_SHARED_TOP,0x24u,&v) || (v&A04_TOP_AUDIO_BIT)) {s->poisoned=1;return -1;}
    if(s->restore_gated && (o->write32(o->context,A04_SHARED_TOP,0x54u,A04_TOP_AUDIO_BIT) ||
       o->read32(o->context,A04_SHARED_TOP,0x24u,&v) || !(v&A04_TOP_AUDIO_BIT))) {
        s->poisoned=1;return -1;
    }
    s->held=0;s->restore_gated=0;return 0;
}
