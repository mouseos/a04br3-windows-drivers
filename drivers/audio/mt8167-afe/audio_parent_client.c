#include "audio_parent_client.h"
static int parent_ref(void *v) { struct Mt8167AudioParentClient *c=v; if(!c->bound || c->clients==~0u)return -1;c->parent.Header.InterfaceReference(c->parent.Header.Context);c->clients++;return 0; }
static void parent_deref(void *v) { struct Mt8167AudioParentClient *c=v;if(c->clients){c->clients--;c->parent.Header.InterfaceDereference(c->parent.Header.Context);} }
static int parent_acquire(void *v,uint32_t rate) { struct Mt8167AudioParentClient *c=v;if(!c->bound)return -1;return c->parent.AcquireMuted(c->parent.Header.Context,rate); }
static int parent_release(void *v) { struct Mt8167AudioParentClient *c=v;if(!c->bound)return -1;return c->parent.ReleaseQuiescent(c->parent.Header.Context); }
int Mt8167AudioParentBind(struct Mt8167AudioParentClient *c,const A04_SHARED_AUDIO_INTERFACE *p)
{
    if(!c || !p || c->bound || c->clients || p->Header.Size!=sizeof(*p) ||
       p->Header.Version!=A04_SHARED_AUDIO_INTERFACE_VERSION || !p->Header.Context ||
       !p->Header.InterfaceReference || !p->Header.InterfaceDereference ||
       !p->AcquireMuted || !p->ReleaseQuiescent || !p->PrepareMaskedIrq || !p->BindAfe || !p->Mute) return -1;
    c->parent=*p;c->service.size=sizeof(c->service);c->service.version=1;c->service.context=c;
    c->service.reference=parent_ref;c->service.dereference=parent_deref;
    c->service.acquire_muted=parent_acquire;c->service.release_quiescent=parent_release;
    c->bound=1;return 0;
}
const struct mt8167_audio_shared *Mt8167AudioParentService(struct Mt8167AudioParentClient *c)
{ return c && c->bound ? &c->service : 0; }
int Mt8167AudioParentClose(struct Mt8167AudioParentClient *c)
{
    if(!c || !c->bound || c->clients)return -1;
    /* Caller must ensure no attached client can still reach this wrapper. */
    c->bound=0;c->parent.Header.InterfaceDereference(c->parent.Header.Context);return 0;
}
