/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_VOICE_PLAYBACK_H
#define A04_MT8167_VOICE_PLAYBACK_H
#include "../../bus/mt8167-shared/codec_voice.h"
/* Experimental composition. The serialized caller retains every resource after
 * any attempted start. The provider must establish exclusive ownership, a
 * real downstream mute, calibrated voice-path DC, and live PCM DMA coherence.
 * None of these callbacks is itself evidence of those platform properties. */
enum a04_voice_playback_state { A04_VOICE_READY, A04_VOICE_RUNNING, A04_VOICE_RETAINED };
struct a04_voice_playback {
 struct a04_codec_voice voice;
 enum a04_voice_playback_state state;
};
struct a04_voice_playback_io {
 struct a04_codec_voice_io codec;
 void *context;
 /* A successful preflight proves all prerequisites without changing hardware. */
 int (*preflight)(void *);
 int (*dma_start)(void *);
 int (*dma_stop)(void *);
 int (*unmute)(void *);
 int (*mute)(void *);
};
static int a04_voice_playback_start(struct a04_voice_playback *s,const struct a04_voice_playback_io *io)
{
 if(!s || !io || s->state!=A04_VOICE_READY || !io->preflight || !io->dma_start ||
    !io->dma_stop || !io->unmute || !io->mute || io->preflight(io->context))return -1;
 /* An ambiguous codec write, DMA start or unmute requires retaining ownership. */
 s->state=A04_VOICE_RETAINED;
 if(a04_codec_voice_start(&s->voice,&io->codec))return -1;
 if(io->dma_start(io->context)) { (void)io->dma_stop(io->context); return -1; }
 if(io->unmute(io->context)) {
  (void)io->mute(io->context); (void)io->dma_stop(io->context); return -1;
 }
 s->state=A04_VOICE_RUNNING;return 0;
}
static int a04_voice_playback_stop(struct a04_voice_playback *s,const struct a04_voice_playback_io *io)
{
 int mute,stop;
 if(!s || !io || s->state!=A04_VOICE_RUNNING || !io->mute || !io->dma_stop)return -1;
 s->state=A04_VOICE_RETAINED;
 /* Stop DMA even if mute is uncertain; never turn off the analog path unless
 * both operations succeeded and the codec helper re-verifies mute. */
 mute=io->mute(io->context);stop=io->dma_stop(io->context);
 if(mute || stop)return -1;
 return a04_codec_voice_stop(&s->voice,&io->codec);
}
#endif
