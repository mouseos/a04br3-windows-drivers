/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_CODEC_VOICE_H
#define A04_MT8167_CODEC_VOICE_H
#include "codec_domain.h"
#include "codec_dc.h"
/* The caller must hold exclusive AFE/APMIXED ownership, the codec domain,
 * and a verified downstream mute. Calibration must come from the board's
 * Linux-compatible measurement; this module does not obtain or validate it. */
struct a04_codec_voice {
 struct a04_codec_dc dc;
 int active, poisoned;
 uint32_t gain;
 int16_t measured;
 unsigned cap;
};
struct a04_codec_voice_io {
 struct a04_codec_domain_io analog;
 struct a04_codec_dc_io afe;
 int (*verify_muted_and_calibrated)(void *,int16_t *,unsigned *);
};
static int a04_codec_voice_start(struct a04_codec_voice *s,const struct a04_codec_voice_io *io)
{
 uint32_t con1,con2,afe0; int16_t measured; unsigned cap;
 if(!s || !io || s->active || s->poisoned || !io->verify_muted_and_calibrated ||
    !io->analog.read || !io->analog.write || !io->afe.read || !io->afe.write || !io->afe.delay_us ||
    io->verify_muted_and_calibrated(io->analog.context,&measured,&cap) || cap>3 ||
    io->analog.read(io->analog.context,0x704,&con1) ||
    io->analog.read(io->analog.context,0x708,&con2) ||
    io->afe.read(io->afe.context,0xf00,&afe0) ||
    (con1&(1u<<15)) || (con2&(1u<<8)) || (afe0&1u)) return -1;
 s->gain=con2&(15u<<9);s->measured=measured;s->cap=cap;s->active=1;
 /* Linux AIF RX, DAC PRE/POST_PMU, then Voice Amp depop. */
 if(io->afe.write(io->afe.context,0xf00,afe0|1u) ||
    a04_codec_dc_set(&s->dc,&io->afe,0x4b4b) ||
    a04_codec_domain_update(&io->analog,0x704,1u<<15,0,1u<<15) ||
    a04_codec_dc_ramp(&s->dc,&io->afe,0x4b4b,measured,cap) ||
    a04_codec_domain_update(&io->analog,0x708,15u<<9,s->gain,0) ||
    a04_codec_domain_update(&io->analog,0x708,1u<<8,0,1u<<8) ||
    a04_codec_domain_update(&io->analog,0x708,15u<<9,0,s->gain)) {s->poisoned=1;return -1;}
 return 0;
}
static int a04_codec_voice_stop(struct a04_codec_voice *s,const struct a04_codec_voice_io *io)
{
 uint32_t con2,afe0;
 if(!s || !io || !s->active || s->poisoned || !io->verify_muted_and_calibrated ||
    !io->analog.read || !io->analog.write || !io->afe.read || !io->afe.write || !io->afe.delay_us) return -1;
 /* Caller re-establishes downstream mute before stop. */
 {int16_t measured;unsigned cap;
  if(io->verify_muted_and_calibrated(io->analog.context,&measured,&cap) || measured!=s->measured || cap!=s->cap)return -1;}
 if(io->analog.read(io->analog.context,0x708,&con2) || (con2&(15u<<9))!=s->gain ||
    io->afe.read(io->afe.context,0xf00,&afe0) || !(afe0&1u)) {s->poisoned=1;return -1;}
 if(a04_codec_domain_update(&io->analog,0x708,15u<<9,s->gain,0) ||
    a04_codec_domain_update(&io->analog,0x708,1u<<8,1u<<8,0) ||
    a04_codec_domain_update(&io->analog,0x708,15u<<9,0,s->gain) ||
    a04_codec_dc_ramp(&s->dc,&io->afe,s->measured,0x4b4b,s->cap) ||
    a04_codec_domain_update(&io->analog,0x704,1u<<15,1u<<15,0) ||
    a04_codec_dc_set(&s->dc,&io->afe,0) ||
    io->afe.write(io->afe.context,0xf00,afe0&~1u)) {s->poisoned=1;return -1;}
 s->active=0;return 0;
}
#endif
