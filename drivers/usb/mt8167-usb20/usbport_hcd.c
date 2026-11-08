/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "usbport81.h"
#include "root_hub.h"
#include "pio_transfer.h"
/* USBPORT owns its FDO, root PDO, USBDI interfaces, address and pipe handles.
 * This module never creates a competing root PDO or completes a raw IRP. */
struct a04_hcd;
struct a04_mp_endpoint {struct a04_mp_properties p;ULONG state,status,toggle;};
struct a04_mp_transfer {struct a04_mp_parameters *parameters;ULONG bytes,actual,state;};
/* The physical backend is deliberately versioned separately from USBPORT's
 * private ABI. It must prove the existing board admission plus real voltage
 * and overcurrent validity; v1 Health() is NOT upgraded into that proof. */
#define A04_HCD_BACKEND_VERSION 1
struct a04_hcd_backend {
 ULONG version,bytes;PVOID context;
 ULONG (*start)(PVOID,struct a04_mp_resources *,MT8167_USB20_IO *);
 ULONG (*stop)(PVOID);
 ULONG (*power)(PVOID,int,struct a04_hub_observation *);
 ULONG (*reset)(PVOID,struct a04_hub_observation *);
 ULONG (*control)(PVOID,struct a04_mp_endpoint *,struct a04_mp_parameters *,struct a04_mp_transfer *);
 ULONG (*abort)(PVOID,struct a04_mp_endpoint *,struct a04_mp_transfer *,PULONG);
 VOID (*poll)(PVOID);ULONG (*frame)(PVOID);
};
struct a04_hcd {
 ULONG signature,started,irq_enabled,root_irq,fault;
 struct a04_root_port port;struct a04_hub hub;
 struct a04_hcd_backend backend;struct a04_mp_resources resources;MT8167_USB20_IO io;
 struct a04_mp_endpoint *active_ep;struct a04_mp_transfer *active;
 struct a04_pio_transfer pio;
};
static struct a04_mp_packet Packet;
#define CTX(p) ((struct a04_hcd *)(p))
static VOID RhData(PVOID p,struct a04_mp_root *d)
{UNREFERENCED_PARAMETER(p);d->ports=1;d->characteristics=9;d->pad=0;d->power_good=50;d->current=0;}
static ULONG RhStatus(PVOID p,PUSHORT s)
{/* RT81 USBEHCI returns 1 in this USHORT: device remote-wakeup status, not port bitmap. */
 UNREFERENCED_PARAMETER(p);if(!s)return A04_MP_ERROR;*s=1;return 0;}
static ULONG RhPort(PVOID p,USHORT port,PULONG s)
{struct a04_hcd *h=CTX(p);if(port!=1||!s)return A04_MP_ERROR;*s=h->port.status|(h->port.change<<16);return 0;}
static ULONG RhHub(PVOID p,PULONG s)
{UNREFERENCED_PARAMETER(p);if(!s)return 1;*s=0;return 0;}
static ULONG RhFeature(struct a04_hcd *h,USHORT port,unsigned set,unsigned feature)
{
 unsigned char setup[8]={0x23,0,0,0,1,0,0,0};unsigned actual;int r;
 struct a04_hub_action a;struct a04_hub_observation o;
 if(port!=1||!h->started||h->fault)return A04_MP_ERROR;
 setup[1]=(UCHAR)(set?3:1);setup[2]=(UCHAR)feature;
 r=a04_hub_control(&h->hub,setup,0,0,&actual,&a);
 if(r==A04_HUB_OK)return 0;
 if(r!=A04_HUB_PENDING)return A04_MP_UNSUPPORTED;
 RtlZeroMemory(&o,sizeof(o));
 /* No backend means a present but unpowered root port. Never report an
  * unperformed POWER/RESET as success just to let hub enumeration progress. */
 if(h->backend.version!=A04_HCD_BACKEND_VERSION||!h->backend.power||!h->backend.reset){
  (void)a04_hub_action_done(&h->hub,&a,0,&o);return A04_MP_UNSUPPORTED;
 }
 if(a.kind==A04_HUB_RESET)r=h->backend.reset(h->backend.context,&o);
 else r=h->backend.power(h->backend.context,a.kind==A04_HUB_POWER_ON,&o);
 return a04_hub_action_done(&h->hub,&a,r==0,&o)?A04_MP_HARDWARE:0;
}
#define FEATURE(name,set,feature) static ULONG name(PVOID p,USHORT n){return RhFeature(CTX(p),n,set,feature);}
FEATURE(RhReset,1,4) FEATURE(RhPower,1,8) FEATURE(RhEnable,1,1) FEATURE(RhSuspend,1,2)
FEATURE(RhClearEnable,0,1) FEATURE(RhClearPower,0,8) FEATURE(RhClearSuspend,0,2)
FEATURE(RhClearEnableChange,0,17) FEATURE(RhClearConnectChange,0,16)
FEATURE(RhClearResetChange,0,20) FEATURE(RhClearSuspendChange,0,18) FEATURE(RhClearOcChange,0,19)
static VOID RhDisable(PVOID p){CTX(p)->root_irq=0;}
static VOID RhEnableIrq(PVOID p){CTX(p)->root_irq=1;}
static ULONG Start(PVOID p,struct a04_mp_resources *r)
{
 struct a04_hcd *h=CTX(p);
 if(!r||!r->fdo||!r->pdo)return A04_MP_ERROR;
 RtlZeroMemory(h,sizeof(*h));h->signature=0x34484344;
 a04_port_init(&h->port);if(a04_hub_init(&h->hub,&h->port,50))return A04_MP_ERROR;
 /* Software HCD/root-hub lifecycle is online; physical bus remains OFF.
  * No firmware clock, PHY, DMA, ID, GPIO or VBUS is touched by this start.
  * Physical backend is installed only by an independently qualified owner. */
 h->resources=*r;h->started=1;return 0;
}
static VOID Stop(PVOID p,BOOLEAN removed)
{
 struct a04_hcd *h=CTX(p);UNREFERENCED_PARAMETER(removed);
 h->irq_enabled=h->root_irq=0;a04_hub_invalidate(&h->hub);
 if(h->active){h->fault=1;return;} /* Never free DMA/endpoint ownership on a guessed drain. */
 if(h->backend.stop&&h->backend.stop(h->backend.context)){h->fault=1;return;}
 (void)a04_port_power(&h->port,0,0);h->started=0;
}
static VOID Suspend(PVOID p){Stop(p,FALSE);}
static ULONG Resume(PVOID p){return CTX(p)->started?0:A04_MP_UNSUPPORTED;}
static BOOLEAN Isr(PVOID p){UNREFERENCED_PARAMETER(p);return FALSE;}
static VOID Dpc(PVOID p,BOOLEAN enabled){UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(enabled);}
static VOID Enable(PVOID p){CTX(p)->irq_enabled=1;}
static VOID Disable(PVOID p){CTX(p)->irq_enabled=0;}
static VOID Check(PVOID p){UNREFERENCED_PARAMETER(p);}
static ULONG Frame(PVOID p){struct a04_hcd *h=CTX(p);return h->backend.frame?h->backend.frame(h->backend.context):0;}
static VOID NextSof(PVOID p){UNREFERENCED_PARAMETER(p);}
static VOID Reset(PVOID p){struct a04_hcd *h=CTX(p);a04_hub_invalidate(&h->hub);h->fault=1;}
static VOID Query(PVOID p,struct a04_mp_properties *e,struct a04_mp_requirements *q)
{UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(e);q->common_bytes=0;q->max_transfer=65535;}
static ULONG Open(PVOID p,struct a04_mp_properties *v,PVOID endpoint)
{
 struct a04_mp_endpoint *e=endpoint;UNREFERENCED_PARAMETER(p);
 if(!v||!e||v->address>127||v->endpoint>15||v->hub||v->port||v->speed>2||!v->packet||v->packet>64||
  (v->type!=1&&v->type!=2&&v->type!=3)||v->direction>1||v->transactions>1)return A04_MP_UNSUPPORTED;
 if(v->speed==0&&(v->type==2||v->packet>8))return A04_MP_UNSUPPORTED;
 if(v->type==2&&v->speed!=1)return A04_MP_UNSUPPORTED;
 RtlZeroMemory(e,sizeof(*e));e->p=*v;e->state=A04_EP_PAUSED;return 0;
}
static ULONG Reopen(PVOID p,struct a04_mp_properties *v,PVOID endpoint)
{if(CTX(p)->active_ep==endpoint)return A04_MP_ERROR;return Open(p,v,endpoint);}
static VOID Close(PVOID p,PVOID endpoint,BOOLEAN removed)
{struct a04_hcd *h=CTX(p);struct a04_mp_endpoint *e=endpoint;UNREFERENCED_PARAMETER(removed);
 if(h->active_ep==e){h->fault=1;return;}e->state=A04_EP_CLOSED;}
static ULONG State(PVOID p,PVOID endpoint){UNREFERENCED_PARAMETER(p);return ((struct a04_mp_endpoint*)endpoint)->state;}
static VOID SetState(PVOID p,PVOID endpoint,ULONG state)
{struct a04_mp_endpoint *e=endpoint;struct a04_hcd *h=CTX(p);
 if(state!=A04_EP_ACTIVE&&state!=A04_EP_PAUSED&&state!=A04_EP_REMOVE)return;
 if(h->active_ep==e&&state!=A04_EP_ACTIVE){h->fault=1;return;}e->state=state;}
static ULONG EpStatus(PVOID p,PVOID endpoint){UNREFERENCED_PARAMETER(p);return ((struct a04_mp_endpoint*)endpoint)->status;}
static VOID SetStatus(PVOID p,PVOID endpoint,ULONG status)
{UNREFERENCED_PARAMETER(p);((struct a04_mp_endpoint*)endpoint)->status=status;}
static VOID Toggle(PVOID p,PVOID endpoint,ULONG toggle)
{struct a04_hcd *h=CTX(p);if(toggle>1||h->active_ep==endpoint){h->fault=1;return;}((struct a04_mp_endpoint*)endpoint)->toggle=toggle;}
static ULONG Submit(PVOID p,PVOID endpoint,struct a04_mp_parameters *v,PVOID transfer,struct a04_mp_sg *sg)
{
 struct a04_hcd *h=CTX(p);struct a04_mp_endpoint *e=endpoint;struct a04_mp_transfer *t=transfer;
 struct a04_pio_endpoint pe;
 if(!v||!t||!e||h->fault||!h->started||!h->backend.version||!Packet.complete||h->active||
  e->state!=A04_EP_ACTIVE||e->status||v->bytes>65535||
  (h->port.status&(A04_PORT_POWER|A04_PORT_CONNECTION|A04_PORT_ENABLE|A04_PORT_OVERCURRENT))!=
   (A04_PORT_POWER|A04_PORT_CONNECTION|A04_PORT_ENABLE))return A04_MP_ERROR;
 RtlZeroMemory(t,sizeof(*t));t->parameters=v;t->bytes=v->bytes;t->state=1;
 if(e->p.type==1){
  if(!h->backend.control)return A04_MP_UNSUPPORTED;
  if(h->backend.control(h->backend.context,e,v,t))return A04_MP_ERROR;
 }else{
  /* Mapped VA is valid only for the owner-held transfer lifetime; endpoint
   * common-buffer mapping services must not be used to map URB data. */
  if(v->bytes&&(!sg||!sg->mapped_va))return A04_MP_ERROR;
  pe.address=e->p.address;pe.endpoint=e->p.endpoint;pe.packet=e->p.packet;
  pe.speed=e->p.speed==2?0x40:e->p.speed==1?0x80:0xc0;
  pe.type=e->p.type;pe.direction=e->p.direction;pe.period=e->p.period;pe.toggle=e->toggle;
  if(a04_pio_configure(&h->io,&h->pio,&pe,sg?(unsigned char*)sg->mapped_va:0,v->bytes))return A04_MP_UNSUPPORTED;
 }
 h->active_ep=e;h->active=t;return 0;
}
static ULONG SubmitIso(PVOID p,PVOID e,struct a04_mp_parameters *v,PVOID t,struct a04_mp_sg *sg)
{UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(e);UNREFERENCED_PARAMETER(v);UNREFERENCED_PARAMETER(t);UNREFERENCED_PARAMETER(sg);return A04_MP_UNSUPPORTED;}
static VOID Abort(PVOID p,PVOID endpoint,PVOID transfer,PULONG actual)
{
 struct a04_hcd *h=CTX(p);struct a04_mp_endpoint *e=endpoint;struct a04_mp_transfer *t=transfer;
 if(!actual)return;*actual=0;
 if(h->active!=t||h->active_ep!=e){h->fault=1;return;}
 if(e->p.type==1){
  if(!h->backend.abort||h->backend.abort(h->backend.context,e,t,actual)){h->fault=1;return;}
 }else{
  if(a04_pio_abort(&h->io,&h->pio)){h->fault=1;return;}
  *actual=h->pio.actual;e->toggle=h->pio.ep.toggle;
 }
 /* USBPORT owns abort completion; do not also call CompleteTransfer. */
 t->state=3;h->active=0;h->active_ep=0;
}
static VOID Poll(PVOID p,PVOID endpoint)
{
 struct a04_hcd *h=CTX(p);struct a04_mp_endpoint *e=endpoint;struct a04_mp_transfer *t=h->active;
 USBD_STATUS status;ULONG actual;int r;
 if(!t||h->active_ep!=e||h->fault)return;
 if(e->p.type==1){if(h->backend.poll)h->backend.poll(h->backend.context);return;}
 r=a04_pio_event(&h->io,&h->pio);if(!r)return;
 actual=h->pio.actual;status=r==1?USBD_STATUS_SUCCESS:r==A04_PIO_STALL?USBD_STATUS_STALL_PID:USBD_STATUS_XACT_ERROR;
 if(r<0&&a04_pio_abort(&h->io,&h->pio)){h->fault=1;return;}
 e->toggle=h->pio.ep.toggle;t->actual=actual;t->state=2;
 h->active=0;h->active_ep=0;
 /* RT81 service signature confirmed from r0/r1/r2/r3 plus stack actual. */
 Packet.complete(h,e,t->parameters,status,actual);
}
static VOID PollController(PVOID p){struct a04_hcd *h=CTX(p);if(h->active_ep)Poll(p,h->active_ep);else if(h->backend.poll)h->backend.poll(h->backend.context);}
/* Exported test seam consumes an already verified backend, never raw request
 * data. Production has no unguarded physical backend or IOCTL to install one. */
ULONG A04HcdSetBackend(PVOID p,const struct a04_hcd_backend *b)
{
 struct a04_hcd *h=CTX(p);if(!b||b->version!=1||b->bytes!=sizeof(*b)||!b->start||!b->stop||
  !b->power||!b->reset||!b->control||!b->abort||!b->poll||!b->frame||h->active||h->fault)return 1;
 if(!h->started||h->backend.version)return 1;
 if(b->start(b->context,&h->resources,&h->io)){h->fault=1;return 1;}
 if(!h->io.read8||!h->io.read16||!h->io.write8||!h->io.write16){
  (void)b->stop(b->context);h->fault=1;return 1;
 }
 h->backend=*b;return 0;
}
/* Backend notification seam. No IOCTL, registry, or user-mode publisher. */
ULONG A04HcdControlComplete(PVOID p,PVOID endpoint,PVOID transfer,USBD_STATUS status,ULONG actual)
{
 struct a04_hcd *h=CTX(p);struct a04_mp_transfer *t=transfer;
 if(!h->active||h->active!=t||h->active_ep!=endpoint||h->active_ep->p.type!=1||actual>t->bytes||!Packet.complete)return 1;
 h->active=0;h->active_ep=0;t->actual=actual;t->state=2;
 Packet.complete(h,endpoint,t->parameters,status,actual);return 0;
}
ULONG A04HcdPortObservation(PVOID p,int connected,const struct a04_hub_observation *o)
{
 struct a04_hcd *h=CTX(p);
 if(!h->started||!h->backend.version||!o||h->fault)return 1;
 if(connected){
  if(!o->qualified||!o->id_host||!o->overcurrent_clear||!o->vbus_verified||!(h->port.status&A04_PORT_POWER))return 1;
 }else{
  /* Invalidate actions first. Active data must be quiesced by backend before
   * its software state is dropped or completion can release buffer lifetime. */
  a04_hub_invalidate(&h->hub);
  if(h->active&&!o->endpoints_quiesced){h->fault=1;return 1;}
 }
 if(a04_port_connect(&h->port,connected))return 1;
 if(h->root_irq&&Packet.invalidate_root)Packet.invalidate_root(h);
 return 0;
}
static VOID FillPacket(void)
{
 RtlZeroMemory(&Packet,sizeof(Packet));Packet.version=3;
 /* 0x2000 is the real RT81 non-PCI/platform path; 0x100 selects no common
  * DMA-buffer requirements. This backend is polled, never drives an IRQ. */
 Packet.flags=0x21b0;Packet.bandwidth=400000;
 Packet.extension_bytes=sizeof(struct a04_hcd);Packet.endpoint_bytes=sizeof(struct a04_mp_endpoint);
 Packet.transfer_bytes=sizeof(struct a04_mp_transfer);RtlCopyMemory(Packet.name,"A04MUSB",7);
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
 if(USBPORT_GetHciMn()!=0x10000001)return STATUS_NOT_SUPPORTED;
 FillPacket();driver->DriverUnload=NULL;
 /* Fourth RegistryPath parameter is mandatory in this actual RT81 build;
  * incompatible public compatibility declarations with only 3 args are not used. */
 return USBPORT_RegisterUSBPortDriver(driver,200,&Packet,path);
}
#endif
