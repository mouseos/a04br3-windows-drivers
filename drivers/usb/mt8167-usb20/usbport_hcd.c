/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "usbport81.h"
#include "root_hub.h"
#include "musb_physical.h"
#include "usbport_profile.h"
/* USBPORT owns the FDO/root PDO, addresses/pipes and completion serialization.
 * Worker never sees these extension pointers. Lock order: HCD -> runtime;
 * PASSIVE worker uses runtime only and never calls back into USBPORT. */
struct a04_mp_endpoint {struct a04_mp_properties p;ULONG state,status,toggle,cookie,toggle_unknown;};
struct a04_mp_transfer {struct a04_mp_parameters *parameters;PVOID mapped;ULONG bytes,actual,state;};
struct a04_hcd {
 KSPIN_LOCK lock;ULONG signature,started,irq_enabled,root_irq,fault,next_cookie,epoch;
 struct a04_root_port port;struct a04_hub hub;struct a04_hub_action root_action;
 struct mt_musb_runtime *runtime;struct a04_mp_endpoint *active_ep;struct a04_mp_transfer *active;
};
static struct a04_mp_packet Packet;
static LONG EpochSeed;
#define CTX(p) ((struct a04_hcd *)(p))
#if defined(A04_HCD_CONTRACT_TEST)
#define HINIT(h) ((void)(h))
#define HLOCK(h,k) ((void)(h),(k)=PASSIVE_LEVEL)
#define HUNLOCK(h,k) ((void)(h),(void)(k))
#else
#define HINIT(h) KeInitializeSpinLock(&(h)->lock)
#define HLOCK(h,k) KeAcquireSpinLock(&(h)->lock,&(k))
#define HUNLOCK(h,k) KeReleaseSpinLock(&(h)->lock,k)
#endif
static void HarvestRoot(struct a04_hcd *h)
{
 struct a04_hub_observation o;ULONG connected=0,done=0,result=0;
 if(!h->runtime)return;
 if(MtMusbObserve(h->runtime,&o,&connected,&done,&result)){
  h->fault=1;a04_hub_invalidate(&h->hub);(void)a04_port_connect(&h->port,0);
  if(o.vbus_off_verified||o.source_off_verified)(void)a04_port_power(&h->port,0,0);return;
 }
 if(done){
  if(done!=h->root_action.kind||a04_hub_action_done(&h->hub,&h->root_action,!result,&o))h->fault=1;
  h->root_action.kind=0;
 }
 if((h->port.status&A04_PORT_POWER)&&!!(h->port.status&A04_PORT_CONNECTION)!=!!connected){
  a04_hub_invalidate(&h->hub);h->root_action.kind=0;(void)a04_port_connect(&h->port,connected);
 }
}
static VOID RhData(PVOID p,struct a04_mp_root *d)
{UNREFERENCED_PARAMETER(p);d->ports=1;d->characteristics=9;d->pad=0;d->power_good=50;d->current=0;}
static ULONG RhStatus(PVOID p,PUSHORT s)
{UNREFERENCED_PARAMETER(p);if(!s)return A04_MP_ERROR;*s=1;return 0;}
static ULONG RhPort(PVOID p,USHORT port,PULONG s)
{
 struct a04_hcd *h=CTX(p);KIRQL k;if(port!=1||!s)return A04_MP_ERROR;
 HLOCK(h,k);HarvestRoot(h);*s=h->port.status|(h->port.change<<16);HUNLOCK(h,k);return 0;
}
static ULONG RhHub(PVOID p,PULONG s){UNREFERENCED_PARAMETER(p);if(!s)return 1;*s=0;return 0;}
static ULONG RhFeature(struct a04_hcd *h,USHORT port,unsigned set,unsigned feature)
{
 UCHAR setup[8]={0x23,0,0,0,1,0,0,0};unsigned actual;int r;KIRQL k;
 struct a04_hub_action a;struct a04_hub_observation o;
 if(port!=1)return A04_MP_ERROR;
 HLOCK(h,k);HarvestRoot(h);
 if(!h->started||(!set&&feature>=16?0:h->fault)||!h->runtime){HUNLOCK(h,k);return A04_MP_HARDWARE;}
 setup[1]=(UCHAR)(set?3:1);setup[2]=(UCHAR)feature;
 r=a04_hub_control(&h->hub,setup,0,0,&actual,&a);
 if(r==A04_HUB_PENDING){
  if(h->active||MtMusbRootQueue(h->runtime,a.kind)){
   RtlZeroMemory(&o,sizeof(o));(void)a04_hub_action_done(&h->hub,&a,0,&o);r=A04_HUB_STALL;
  }else {h->root_action=a;r=0;}
 }
 HUNLOCK(h,k);
 /* Accepted command is asynchronous; POWER/ENABLE/RESET_CHANGE bits are
  * published only after PASSIVE physical observation, not on this return. */
 return r==0?0:A04_MP_UNSUPPORTED;
}
#define FEATURE(name,set,feature) static ULONG name(PVOID p,USHORT n){return RhFeature(CTX(p),n,set,feature);}
FEATURE(RhReset,1,4) FEATURE(RhPower,1,8) FEATURE(RhEnable,1,1) FEATURE(RhSuspend,1,2)
FEATURE(RhClearEnable,0,1) FEATURE(RhClearPower,0,8) FEATURE(RhClearSuspend,0,2)
FEATURE(RhClearEnableChange,0,17) FEATURE(RhClearConnectChange,0,16)
FEATURE(RhClearResetChange,0,20) FEATURE(RhClearSuspendChange,0,18) FEATURE(RhClearOcChange,0,19)
static VOID RhDisable(PVOID p){struct a04_hcd *h=CTX(p);KIRQL k;HLOCK(h,k);h->root_irq=0;HUNLOCK(h,k);}
static VOID RhEnableIrq(PVOID p){struct a04_hcd *h=CTX(p);KIRQL k;HLOCK(h,k);h->root_irq=1;HUNLOCK(h,k);}
/* Actual RT81 legacy async service copies its payload synchronously and
 * invokes callbacks (also during cancellation) at DISPATCH. USBPORT retains
 * the extension until its async timer rundown. This is NOT our worker. */
static VOID Stop(PVOID p,BOOLEAN removed);
static VOID AsyncPoll(PVOID p,PVOID copied_epoch)
{
 struct a04_hcd *h=CTX(p);KIRQL k;ULONG epoch=*(ULONG *)copied_epoch,run;
 HLOCK(h,k);run=h->started&&h->epoch==epoch;HUNLOCK(h,k);
 if(!run)return;
 /* Reason 3 is observed to enqueue the simulated-interrupt DPC, which
  * schedules USBPORT's own endpoint poll under its normal serialization. */
 Packet.invalidate_controller(h,3);
 if(!NT_SUCCESS(Packet.async_callback(h,10,&epoch,sizeof(epoch),AsyncPoll))){
  HLOCK(h,k);h->fault=1;HUNLOCK(h,k);Stop(h,FALSE);
 }
}
static ULONG Start(PVOID p,struct a04_mp_resources *r)
{
 struct a04_hcd *h=CTX(p);NTSTATUS s;
 if(!r||!r->fdo||!r->pdo||!Packet.async_callback||!Packet.invalidate_controller||!Packet.invalidate_root)return A04_MP_ERROR;
 RtlZeroMemory(h,sizeof(*h));HINIT(h);h->signature=0x34484344;
 a04_port_init(&h->port);if(a04_hub_init(&h->hub,&h->port,50))return A04_MP_ERROR;
 s=MtMusbOpen(r->fdo,&h->runtime);
 if(!NT_SUCCESS(s)){h->fault=1;return s==STATUS_NOT_SUPPORTED?A04_MP_UNSUPPORTED:A04_MP_HARDWARE;}
#if defined(A04_HCD_CONTRACT_TEST)
 h->epoch=(ULONG)++EpochSeed;
#else
 h->epoch=(ULONG)InterlockedIncrement(&EpochSeed);
#endif
 h->started=1;
 if(!NT_SUCCESS(Packet.async_callback(h,10,&h->epoch,sizeof(h->epoch),AsyncPoll))){
  struct mt_musb_runtime *runtime=h->runtime;h->runtime=NULL;h->started=0;h->fault=1;
  (void)MtMusbClose(runtime);return A04_MP_RESOURCES;
 }
 return 0;
}
static VOID Stop(PVOID p,BOOLEAN removed)
{
 struct a04_hcd *h=CTX(p);struct mt_musb_runtime *runtime;KIRQL k;UNREFERENCED_PARAMETER(removed);
 HLOCK(h,k);h->started=0;h->irq_enabled=h->root_irq=0;a04_hub_invalidate(&h->hub);
 runtime=h->runtime;h->runtime=NULL;
 /* USBPORT may release every external object on return. Only the independent
  * shadow job survives until the PASSIVE drain/definitive stop proof. */
 h->active=NULL;h->active_ep=NULL;HUNLOCK(h,k);
 if(!MtMusbClose(runtime)){HLOCK(h,k);h->fault=1;HUNLOCK(h,k);}
}
static VOID Suspend(PVOID p){Stop(p,FALSE);}
static ULONG Resume(PVOID p){UNREFERENCED_PARAMETER(p);return A04_MP_UNSUPPORTED;}
static BOOLEAN Isr(PVOID p){UNREFERENCED_PARAMETER(p);return FALSE;}
static VOID Dpc(PVOID p,BOOLEAN enabled){UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(enabled);}
static VOID Enable(PVOID p){struct a04_hcd *h=CTX(p);KIRQL k;HLOCK(h,k);h->irq_enabled=1;HUNLOCK(h,k);}
static VOID Disable(PVOID p){struct a04_hcd *h=CTX(p);KIRQL k;HLOCK(h,k);h->irq_enabled=0;HUNLOCK(h,k);}
static VOID Check(PVOID p){struct a04_hcd *h=CTX(p);KIRQL k;HLOCK(h,k);HarvestRoot(h);HUNLOCK(h,k);}
static ULONG Frame(PVOID p){struct a04_hcd *h=CTX(p);KIRQL k;ULONG n;HLOCK(h,k);n=MtMusbFrame(h->runtime);HUNLOCK(h,k);return n;}
static VOID NextSof(PVOID p){UNREFERENCED_PARAMETER(p);}
static VOID Reset(PVOID p){Stop(p,FALSE);}
static VOID Query(PVOID p,struct a04_mp_properties *e,struct a04_mp_requirements *q)
{UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(e);q->common_bytes=0;q->max_transfer=MT_MUSB_SHADOW_BYTES;}
static ULONG Open(PVOID p,struct a04_mp_properties *v,PVOID endpoint)
{
 struct a04_hcd *h=CTX(p);struct a04_mp_endpoint *e=endpoint;KIRQL k;unsigned interval;
 if(!v||!e||v->address>127||(v->endpoint&~0x8fu)||!a04_usbport_native_route(v->hub)||v->speed>2||!v->packet||v->packet>512||
  (v->type!=1&&v->type!=2&&v->type!=3)||v->direction>1||v->transactions>1)return A04_MP_UNSUPPORTED;
 if(a04_usbport_interval(v->speed,v->type,v->period,&interval))return A04_MP_UNSUPPORTED;
 if((v->type==1&&(v->endpoint&15))||(v->type!=1&&!(v->endpoint&15)))return A04_MP_UNSUPPORTED;
 if(v->type!=1&&((v->endpoint&0x80)?0u:1u)!=v->direction)return A04_MP_UNSUPPORTED;
 if(v->speed==0&&(v->type==2||v->packet>8))return A04_MP_UNSUPPORTED;
 if(v->type!=2&&v->packet>64)return A04_MP_UNSUPPORTED;
 if(v->type==2&&!((v->speed==2&&v->packet==512)||
   (v->speed==1&&(v->packet==8||v->packet==16||v->packet==32||v->packet==64))))return A04_MP_UNSUPPORTED;
 HLOCK(h,k);RtlZeroMemory(e,sizeof(*e));e->p=*v;e->state=A04_EP_PAUSED;
 if(++h->next_cookie==0)++h->next_cookie;e->cookie=h->next_cookie;HUNLOCK(h,k);return 0;
}
static ULONG Reopen(PVOID p,struct a04_mp_properties *v,PVOID endpoint)
{
 struct a04_hcd *h=CTX(p);KIRQL k;ULONG busy;HLOCK(h,k);busy=h->active_ep==endpoint;HUNLOCK(h,k);
 return busy?A04_MP_ERROR:Open(p,v,endpoint);
}
static void Detach(struct a04_hcd *h,struct a04_mp_endpoint *e)
{
 if(h->active_ep==e){MtMusbCancel(h->runtime,e->cookie);h->active_ep=NULL;h->active=NULL;e->toggle_unknown=1;}
}
static VOID Close(PVOID p,PVOID endpoint,BOOLEAN removed)
{struct a04_hcd *h=CTX(p);struct a04_mp_endpoint *e=endpoint;KIRQL k;UNREFERENCED_PARAMETER(removed);
 HLOCK(h,k);Detach(h,e);e->state=A04_EP_CLOSED;HUNLOCK(h,k);}
static ULONG State(PVOID p,PVOID endpoint)
{struct a04_hcd *h=CTX(p);KIRQL k;ULONG s;HLOCK(h,k);s=((struct a04_mp_endpoint *)endpoint)->state;HUNLOCK(h,k);return s;}
static VOID SetState(PVOID p,PVOID endpoint,ULONG state)
{
 struct a04_hcd *h=CTX(p);struct a04_mp_endpoint *e=endpoint;KIRQL k;
 if(state!=A04_EP_ACTIVE&&state!=A04_EP_PAUSED&&state!=A04_EP_REMOVE)return;
 HLOCK(h,k);
 /* Pause requests physical cancellation without retaining external pointers
  * in the worker. USBPORT keeps its transfer until the following Abort. */
 if(h->active_ep==e&&state!=A04_EP_ACTIVE){MtMusbCancel(h->runtime,e->cookie);e->toggle_unknown=1;}
 if(h->active_ep==e&&state==A04_EP_ACTIVE&&e->state!=A04_EP_ACTIVE)h->fault=1;
 if(state==A04_EP_REMOVE)Detach(h,e);e->state=state;HUNLOCK(h,k);
}
static ULONG EpStatus(PVOID p,PVOID endpoint)
{struct a04_hcd *h=CTX(p);KIRQL k;ULONG s;HLOCK(h,k);s=((struct a04_mp_endpoint *)endpoint)->status;HUNLOCK(h,k);return s;}
static VOID SetStatus(PVOID p,PVOID endpoint,ULONG status)
{struct a04_hcd *h=CTX(p);KIRQL k;HLOCK(h,k);((struct a04_mp_endpoint *)endpoint)->status=status;HUNLOCK(h,k);}
static VOID Toggle(PVOID p,PVOID endpoint,ULONG toggle)
{
 struct a04_hcd *h=CTX(p);struct a04_mp_endpoint *e=endpoint;KIRQL k;HLOCK(h,k);
 if(toggle<=1&&h->active_ep!=e){e->toggle=toggle;e->toggle_unknown=0;}else h->fault=1;
 HUNLOCK(h,k);
}
static ULONG Submit(PVOID p,PVOID endpoint,struct a04_mp_parameters *v,PVOID transfer,struct a04_mp_sg *sg)
{
 struct a04_hcd *h=CTX(p);struct a04_mp_endpoint *e=endpoint;struct a04_mp_transfer *t=transfer;KIRQL k;ULONG result=A04_MP_ERROR;
 if(!v||!t||!e||v->bytes>MT_MUSB_SHADOW_BYTES||(v->bytes&&(!sg||!sg->mapped_va)))return A04_MP_ERROR;
 if(e->p.type==1&&(v->setup[6]|((unsigned)v->setup[7]<<8))!=v->bytes)return A04_MP_ERROR;
 HLOCK(h,k);HarvestRoot(h);
 if(!h->fault&&h->started&&h->runtime&&Packet.complete&&!h->active&&e->state==A04_EP_ACTIVE&&!e->status&&
  (h->port.status&(A04_PORT_POWER|A04_PORT_CONNECTION|A04_PORT_ENABLE|A04_PORT_OVERCURRENT))==
  (A04_PORT_POWER|A04_PORT_CONNECTION|A04_PORT_ENABLE)){
  if(e->toggle_unknown&&!MtMusbRecoverToggle(h->runtime,e->cookie,&e->toggle))e->toggle_unknown=0;
  if(!e->toggle_unknown&&!MtMusbSubmit(h->runtime,e->cookie,&e->p,e->toggle,v,sg?sg->mapped_va:NULL)){
   RtlZeroMemory(t,sizeof(*t));t->parameters=v;t->mapped=sg?sg->mapped_va:NULL;t->bytes=v->bytes;t->state=1;
   h->active_ep=e;h->active=t;result=0;
  }
 }
 HUNLOCK(h,k);return result;
}
static ULONG SubmitIso(PVOID p,PVOID e,struct a04_mp_parameters *v,PVOID t,struct a04_mp_sg *sg)
{UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(e);UNREFERENCED_PARAMETER(v);UNREFERENCED_PARAMETER(t);UNREFERENCED_PARAMETER(sg);return A04_MP_UNSUPPORTED;}
static VOID Abort(PVOID p,PVOID endpoint,PVOID transfer,PULONG actual)
{
 struct a04_hcd *h=CTX(p);struct a04_mp_endpoint *e=endpoint;KIRQL k;
 if(actual)*actual=0;HLOCK(h,k);
 if(h->active==transfer&&h->active_ep==e)Detach(h,e);
 /* No inbound data was copied from shadow. USBPORT owns abort completion.
  * Detach releases all external pointers immediately, independent of HW drain. */
 HUNLOCK(h,k);
}
static VOID Poll(PVOID p,PVOID endpoint)
{
 struct a04_hcd *h=CTX(p);struct a04_mp_endpoint *e=endpoint;struct a04_mp_transfer *t;
 KIRQL k;ULONG actual=0,status=0,toggle=0;struct a04_mp_parameters *parameters=NULL;
 HLOCK(h,k);HarvestRoot(h);t=h->active;
 if(t&&h->active_ep==e&&e->state==A04_EP_ACTIVE&&h->runtime&&
    !MtMusbTake(h->runtime,e->cookie,t->mapped,t->bytes,&actual,&status,&toggle)){
  t->actual=actual;t->state=2;e->toggle=toggle;parameters=t->parameters;
  h->active=NULL;h->active_ep=NULL;
 }
 HUNLOCK(h,k);
 /* USBPORT calls PollEndpoint under its endpoint serialization. Do not call
  * Complete while holding our lock: completion may reenter miniport methods. */
 if(parameters)Packet.complete(h,e,parameters,status,actual);
}
static VOID PollController(PVOID p)
{
 /* No cached endpoint pointer escapes the HCD lock. Completion stays in the
  * USBPORT-owned PollEndpoint path; no unproved invalidate_endpoint ABI. */
 struct a04_hcd *h=CTX(p);KIRQL k;ULONG notify;
 HLOCK(h,k);HarvestRoot(h);notify=h->root_irq&&h->port.change;HUNLOCK(h,k);
 if(notify&&Packet.invalidate_root)Packet.invalidate_root(h);
}
static VOID FillPacket(void)
{
 RtlZeroMemory(&Packet,sizeof(Packet));Packet.version=3;Packet.flags=0x21b0;Packet.bandwidth=400000;
 Packet.extension_bytes=sizeof(struct a04_hcd);Packet.endpoint_bytes=sizeof(struct a04_mp_endpoint);
 Packet.transfer_bytes=sizeof(struct a04_mp_transfer);RtlCopyMemory(Packet.name,"MT8167",6);
 Packet.open=Open;Packet.reopen=Reopen;Packet.query=Query;Packet.close=Close;
 Packet.start=Start;Packet.stop=Stop;Packet.suspend=Suspend;Packet.resume=Resume;
 Packet.isr=Isr;Packet.dpc=Dpc;Packet.submit=Submit;Packet.submit_iso=SubmitIso;Packet.abort=Abort;
 Packet.get_state=State;Packet.set_state=SetState;Packet.poll_endpoint=Poll;Packet.check=Check;
 Packet.frame=Frame;Packet.next_sof=NextSof;Packet.enable=Enable;Packet.disable=Disable;
 Packet.poll_controller=PollController;Packet.toggle=Toggle;Packet.get_status=EpStatus;Packet.set_status=SetStatus;Packet.reset=Reset;
 Packet.root_data=RhData;Packet.root_status=RhStatus;Packet.port_status=RhPort;Packet.hub_status=RhHub;
 Packet.port_reset=RhReset;Packet.port_power=RhPower;Packet.port_enable=RhEnable;Packet.port_suspend=RhSuspend;
 Packet.clear_enable=RhClearEnable;Packet.clear_power=RhClearPower;Packet.clear_suspend=RhClearSuspend;
 Packet.clear_enable_change=RhClearEnableChange;Packet.clear_connect_change=RhClearConnectChange;
 Packet.clear_reset_change=RhClearResetChange;Packet.clear_suspend_change=RhClearSuspendChange;Packet.clear_oc_change=RhClearOcChange;
 Packet.root_disable=RhDisable;Packet.root_enable=RhEnableIrq;
}
#if !defined(A04_HCD_CONTRACT_TEST)
__declspec(dllimport) ULONG USBPORT_GetHciMn(VOID);
__declspec(dllimport) NTSTATUS USBPORT_RegisterUSBPortDriver(PDRIVER_OBJECT,ULONG,struct a04_mp_packet *,PUNICODE_STRING);
DRIVER_INITIALIZE DriverEntry;
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING path)
{
 NTSTATUS s;if(USBPORT_GetHciMn()!=0x10000001)return STATUS_NOT_SUPPORTED;
 FillPacket();s=USBPORT_RegisterUSBPortDriver(driver,200,&Packet,path);
 /* Retained fault resources/work must not be followed by an unload of their
  * code. This development miniport is deliberately not unloadable. */
 if(NT_SUCCESS(s))driver->DriverUnload=NULL;
 return s;
}
#endif
