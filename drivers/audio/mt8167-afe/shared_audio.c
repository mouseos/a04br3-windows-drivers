#include "shared_audio.h"
int mt8167_audio_shared_attach(struct mt8167_audio_shared_client *c,const struct mt8167_audio_shared *s)
{
    if (!c || !s || c->referenced || c->leased || c->quarantined || s->version!=1 ||
        s->size!=sizeof(*s) || !s->context || !s->reference || !s->dereference ||
        !s->acquire_muted || !s->release_quiescent) return -1;
    if (s->reference(s->context)) return -1;
    c->service=*s;c->referenced=1;return 0;
}
int mt8167_audio_shared_acquire(struct mt8167_audio_shared_client *c,uint32_t rate)
{
    if (!c || !c->referenced || c->leased || c->quarantined || !rate) return -1;
    if (c->service.acquire_muted(c->service.context,rate)) return -1;
    c->leased=1;return 0;
}
int mt8167_audio_shared_release_unexposed(struct mt8167_audio_shared_client *c)
{
    if (!c || !c->referenced || !c->leased || c->quarantined) return -1;
    if (c->service.release_quiescent(c->service.context)) { c->quarantined=1;return -1; }
    c->leased=0;return 0;
}
int mt8167_audio_shared_quarantine(struct mt8167_audio_shared_client *c)
{
    if (!c || !c->referenced || !c->leased || c->quarantined) return -1;
    c->quarantined=1;return 0;
}
int mt8167_audio_shared_detach(struct mt8167_audio_shared_client *c)
{
    if (!c || !c->referenced || c->leased || c->quarantined) return -1;
    c->service.dereference(c->service.context);c->referenced=0;return 0;
}
