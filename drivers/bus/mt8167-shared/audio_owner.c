/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "audio_owner.h"
int a04_audio_owner_acquire(struct a04_audio_owner *o, uint32_t rate)
{
    if (!o || !o->online || o->leased || o->poisoned || !rate ||
        !o->preflight || !o->top_clock_acquire || !o->top_clock_release || !o->clock_acquire || !o->codec_acquire_muted ||
        !o->verify_release || !o->codec_mute || !o->codec_release_quiescent || !o->clock_release) return -1;
    /* Preflight is read-only and must reject without altering hardware. */
    if (o->preflight(o->context,rate)) return -1;
    if (o->top_clock_acquire(o->context,rate)) { o->poisoned=1; return -1; }
    if (o->clock_acquire(o->context,rate)) { o->poisoned=1; return -1; }
    if (o->codec_acquire_muted(o->context)) { o->poisoned=1; return -1; }
    o->leased=1;return 0;
}
int a04_audio_owner_mute(struct a04_audio_owner *o)
{
    if(!o || !o->online || !o->leased || o->poisoned || !o->codec_mute)return -1;
    if(o->codec_mute(o->context)){o->poisoned=1;return -1;}
    return 0;
}
int a04_audio_owner_release(struct a04_audio_owner *o)
{
    if (!o || !o->online || !o->leased || o->poisoned ||
        !o->verify_release || !o->codec_release_quiescent || !o->clock_release || !o->top_clock_release) return -1;
    /* No write or lease release when DMA/IRQ quiescence is unknown. */
    if (o->verify_release(o->context)) return -1;
    if (o->codec_release_quiescent(o->context)) { o->poisoned=1;return -1; }
    if (o->clock_release(o->context)) { o->poisoned=1;return -1; }
    if (o->top_clock_release(o->context)) { o->poisoned=1;return -1; }
    o->leased=0;return 0;
}
void a04_audio_owner_stop(struct a04_audio_owner *o)
{
    if (!o) return;
    o->online=0;
    if (o->leased) o->poisoned=1;
}
