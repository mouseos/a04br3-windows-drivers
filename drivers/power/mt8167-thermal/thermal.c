/* SPDX-License-Identifier: GPL-2.0-only
 * Fixed-point conversions from MediaTek's MT8167 thermal implementation:
 * Linux 4.4 drivers/misc/mediatek/thermal/mt8167/mtk_ts_cpu.c.
 * Calibration words are supplied at runtime; there are no per-device constants.
 */
#include "thermal.h"
#include <stddef.h>

int a04_thermal_calibrate(struct a04_thermal_cal *out, a04_devinfo_read read, void *ctx)
{
    uint32_t w60,w61,w62; int32_t adcge, adcoe, vts[3], slope, sign, id;
    struct a04_thermal_cal c; unsigned i;
    if (!out || !read || read(ctx,60,&w60) || read(ctx,61,&w61) || read(ctx,62,&w62)) return -1;
    if (!(w60&1u) || w60==UINT32_MAX || w61==UINT32_MAX || w62==UINT32_MAX) return -1;
    adcge=(int32_t)((w61>>22)&1023u); adcoe=(int32_t)((w61>>12)&1023u);
    vts[0]=(int32_t)((w60>>17)&511u); vts[1]=(int32_t)((w60>>8)&511u); vts[2]=(int32_t)((w62>>23)&511u);
    c.degc=(int32_t)((w60>>1)&63u); sign=(int32_t)((w60>>7)&1u);
    id=(int32_t)((w61>>2)&1u); slope=id?(int32_t)((w60>>26)&63u):0;
    c.slope=sign?-slope:slope; c.offset=adcoe-512;
    c.gain=10000+(adcge-512)*10000/4096;
    if (c.gain<=0 || 165+c.slope<=0) return -1;
    for (i=0;i<3;i++) c.room[i]=(((vts[i]+3350-c.offset)*10000/4096)*10000)/c.gain;
    *out=c; return 0;
}

int a04_thermal_convert(const struct a04_thermal_cal *c, unsigned sensor, uint16_t raw, int32_t *out)
{
    int32_t diff, stage, temp;
    if (!c || !out || sensor>=3 || raw==0 || raw>4095 || c->gain<=0 ||
        c->gain>20000 || c->slope < -63 || c->slope > 63 ||
        c->offset < -512 || c->offset > 511 || c->degc < 0 || c->degc > 63) return -1;
    diff=(int32_t)raw-c->offset;
    /* Linux's signed arithmetic shift is floor division for a negative value. */
    stage=diff*10000;
    stage=stage>=0 ? stage/4096 : -((-stage+4095)/4096);
    stage=(stage*10000/c->gain-c->room[sensor])*15/18;
    temp=(c->degc*10/2-(stage*100)/(165+c->slope))*100;
    if (temp < -40000 || temp > 150000) return -1;
    *out=temp; return 0;
}

int a04_thermal_sample(const struct a04_thermal_cal *c, const uint32_t raw[3], uint64_t now,
                       int fresh, struct a04_thermal_sample *out)
{
    struct a04_thermal_sample s; unsigned i;
    if (!c || !raw || !out || !fresh) return -1;
    for (i=0;i<3;i++) if(a04_thermal_convert(c,i,(uint16_t)(raw[i]&0xfffu),&s.mc[i])) return -1;
    s.hottest_mc=s.mc[0]; for(i=1;i<3;i++) if(s.mc[i]>s.hottest_mc) s.hottest_mc=s.mc[i];
    s.time_ms=now; *out=s; return 0;
}

enum a04_thermal_action a04_thermal_decide(struct a04_thermal_policy *p,
                                            const struct a04_thermal_sample *s,
                                            uint64_t now, int qualified)
{
    if (!p || !s || !qualified || !p->max_age_ms || p->release_mc>=p->limit_mc ||
        p->limit_mc>=p->halt_mc || now<s->time_ms || now-s->time_ms>p->max_age_ms) return A04_TEMP_HALT;
    if (s->hottest_mc>=p->halt_mc) return A04_TEMP_HALT;
    if (s->hottest_mc>=p->limit_mc) p->limited=1;
    else if (s->hottest_mc<=p->release_mc) p->limited=0;
    return p->limited?A04_TEMP_LIMIT:A04_TEMP_NORMAL;
}
