#include "audio_parent_client.h"
static int parent_ref(void *v) { struct Mt8167AudioParentClient *c=v; if(!c->bound || c->clients==~0u)return -1;c->parent.Header.InterfaceReference(c->parent.Header.Context);c->clients++;return 0; }
static void parent_deref(void *v) { struct Mt8167AudioParentClient *c=v;if(c->clients){c->clients--;c->parent.Header.InterfaceDereference(c->parent.Header.Context);} }
static int parent_acquire(void *v,uint32_t rate) {
 struct Mt8167AudioParentClient *c=v;if(!c->bound)return -1;
#if defined(A04_AUDIO_LIFECYCLE) && A04_AUDIO_LIFECYCLE
 if(!c->lifecycleBound)return -1;
 return NT_SUCCESS(c->lifecycle.AcquireMuted(c->lifecycle.Header.Context,rate,&c->generation))?0:-2;
#else
 return c->parent.AcquireMuted(c->parent.Header.Context,rate);
#endif
}
static int parent_release(void *v) {
 struct Mt8167AudioParentClient *c=v;if(!c->bound)return -1;
#if defined(A04_AUDIO_LIFECYCLE) && A04_AUDIO_LIFECYCLE
 if(!NT_SUCCESS(Mt8167AudioParentCertifyStopped(c)))return -1;
 return NT_SUCCESS(Mt8167AudioParentReleaseStopped(c))?0:-1;
#else
 return c->parent.ReleaseQuiescent(c->parent.Header.Context);
#endif
}
int Mt8167AudioParentBind(struct Mt8167AudioParentClient *c,const A04_SHARED_AUDIO_INTERFACE *p)
{
    if(!c || !p || c->bound || c->clients || p->Header.Size!=sizeof(*p) ||
       p->Header.Version!=A04_SHARED_AUDIO_INTERFACE_VERSION || !p->Header.Context ||
       !p->Header.InterfaceReference || !p->Header.InterfaceDereference ||
       !p->AcquireMuted || !p->ReleaseQuiescent || !p->PrepareMaskedIrq || !p->BindAfe || !p->Mute || !p->UnmuteMin) return -1;
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
#if defined(A04_AUDIO_LIFECYCLE) && A04_AUDIO_LIFECYCLE
    if(c->generation)return -1;
    if(c->lifecycleBound){c->lifecycle.Header.InterfaceDereference(c->lifecycle.Header.Context);c->lifecycleBound=0;}
#endif
    c->bound=0;c->parent.Header.InterfaceDereference(c->parent.Header.Context);return 0;
}

#if defined(A04_AUDIO_LIFECYCLE) && A04_AUDIO_LIFECYCLE
int Mt8167AudioParentBindLifecycle(struct Mt8167AudioParentClient *c,const struct A04_AUDIO_LIFECYCLE_INTERFACE *p)
{
 if(!c || !p || !c->bound || c->lifecycleBound || p->Header.Size!=sizeof(*p) ||
    p->Header.Version!=A04_AUDIO_LIFECYCLE_VERSION || p->Header.Context!=c->parent.Header.Context ||
    !p->Header.InterfaceReference || !p->Header.InterfaceDereference ||
    !p->AcquireMuted || !p->CertifyStopped || !p->ReleaseStopped)return -1;
 c->lifecycle=*p;c->lifecycleBound=1;c->softwareQuiescent=1;return 0;
}
NTSTATUS Mt8167AudioParentCertifyStopped(struct Mt8167AudioParentClient *c)
{
 if(!c || !c->bound || !c->lifecycleBound || !c->generation || !c->softwareQuiescent)return STATUS_DEVICE_NOT_READY;
 return c->lifecycle.CertifyStopped(c->lifecycle.Header.Context,c->generation,A04_AUDIO_STOP_SOFTWARE_QUIESCENT);
}
NTSTATUS Mt8167AudioParentReleaseStopped(struct Mt8167AudioParentClient *c)
{
 NTSTATUS s;
 if(!c || !c->lifecycleBound || !c->generation || !c->softwareQuiescent)return STATUS_DEVICE_NOT_READY;
 s=c->lifecycle.ReleaseStopped(c->lifecycle.Header.Context,c->generation);
 if(NT_SUCCESS(s))c->generation=0;
 return s;
}
#endif
