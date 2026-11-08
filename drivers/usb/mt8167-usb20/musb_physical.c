#include "usbport_profile.h"
/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "musb_physical.h"
#include "board_host.h"
#include "controller.h"
#include "pio_transfer.h"
#include "pnp_resources.h"
#include "../../bus/mt8167-shared/musb_owner_interface.h"
#include "../../bus/mt8167-shared/musb_board_proof.h"
#if defined(MT_MUSB_PHYSICAL_TEST)
#include "../../../tests/usb/musb_physical_os.h"
#endif
#define MT_TAG '31UM'
#define MT_WINDOW 0x10000u
#define MT_FRESH 1000000ull /* 100 ms, same KeQueryInterruptTime time domain */
struct mt_musb_lane {
 struct mt_musb_async job;struct a04_mp_properties properties;
 struct a04_mp_parameters parameters;UCHAR shadow[MT_MUSB_SHADOW_BYTES];
 struct a04_pio_transfer pio;MT8167_USB20_EP0 ep0;BOOLEAN hardware_job;
};
struct mt_musb_runtime {
 KSPIN_LOCK lock;KTIMER timer;KDPC dpc;KEVENT idle;
 PIO_WORKITEM work,cleanup;PDEVICE_OBJECT fdo;
 BOOLEAN queued,closing,lease,fault,started,root_running,io_unsafe;
 volatile UCHAR *mac,*phy;
 MT_MUSB_OWNER_INTERFACE owner;MT_MUSB_OBSERVATION proof;
 MT8167_USB20_CONTROLLER controller;A04_USB_BOARD board;
 struct mt_musb_lane lanes[MT_MUSB_LANES];ULONG cursor;
 struct {ULONG cookie,toggle;} recovered[32];ULONG recovery_next;
 ULONG root_pending,root_done,root_result,connected,frame;
 struct a04_hub_observation observation;
};
static UCHAR Rd8(void *p,unsigned o){return READ_REGISTER_UCHAR(((struct mt_musb_runtime *)p)->mac+o);}
static USHORT Rd16(void *p,unsigned o){return READ_REGISTER_USHORT((volatile USHORT *)(((struct mt_musb_runtime *)p)->mac+o));}
static unsigned Rd32(void *p,unsigned o){return READ_REGISTER_ULONG((volatile ULONG *)(((struct mt_musb_runtime *)p)->mac+o));}
static void Wr8(void *p,unsigned o,UCHAR v){WRITE_REGISTER_UCHAR(((struct mt_musb_runtime *)p)->mac+o,v);}
static void Wr16(void *p,unsigned o,USHORT v){WRITE_REGISTER_USHORT((volatile USHORT *)(((struct mt_musb_runtime *)p)->mac+o),v);}
static void Wr32(void *p,unsigned o,unsigned v){WRITE_REGISTER_ULONG((volatile ULONG *)(((struct mt_musb_runtime *)p)->mac+o),v);}
static UCHAR PhyRead(void *p,unsigned o){return READ_REGISTER_UCHAR(((struct mt_musb_runtime *)p)->phy+0x800+o);}
static unsigned PhyRead32(void *p,unsigned o){return READ_REGISTER_ULONG((volatile ULONG *)(((struct mt_musb_runtime *)p)->phy+0x800+o));}
static void PhyWrite(void *p,unsigned o,UCHAR v){WRITE_REGISTER_UCHAR(((struct mt_musb_runtime *)p)->phy+0x800+o,v);}
static void Delay(void *p,unsigned ms){LARGE_INTEGER t;UNREFERENCED_PARAMETER(p);t.QuadPart=-(LONGLONG)ms*10000;KeDelayExecutionThread(KernelMode,FALSE,&t);
 /* Board calibration/session waits never suppress source safety sampling.
  * Observe can cut GPIO22 on role/OC/VBUS fault; no new source is enabled. */
 if(p){struct mt_musb_runtime *r=p;MT_MUSB_OBSERVATION o;
  if(r->lease&&r->owner.Observe){RtlZeroMemory(&o,sizeof(o));o.bytes=sizeof(o);
   (void)r->owner.Observe(r->owner.Header.Context,&o);}}
}
/* Lease was acquired before PHY use. Board helper must never release it on an
 * internal failure; only Close may release after the definitive stop proof. */
static int BoardPower(void *p,int on){struct mt_musb_runtime *r=p;return on&&r->lease?0:-1;}
static int Vbus(void *p,int on){struct mt_musb_runtime *r=p;return NT_SUCCESS(r->owner.SetVbus(r->owner.Header.Context,(BOOLEAN)on))?0:-1;}
/* WDK81 ntifs.h exposes this reference-returning stack traversal API. Keep
 * its exact public declaration without importing filesystem-only typedefs. */
NTKERNELAPI PDEVICE_OBJECT IoGetLowerDeviceObject(PDEVICE_OBJECT DeviceObject);
#if !defined(MT_MUSB_PHYSICAL_TEST)
static NTSTATUS QueryDone(PDEVICE_OBJECT device,PIRP irp,PVOID context)
{UNREFERENCED_PARAMETER(device);UNREFERENCED_PARAMETER(irp);KeSetEvent((PKEVENT)context,IO_NO_INCREMENT,FALSE);return STATUS_MORE_PROCESSING_REQUIRED;}
static NTSTATUS QueryOwner(PDEVICE_OBJECT fdo,MT_MUSB_OWNER_INTERFACE *owner)
{
 PDEVICE_OBJECT lower=IoGetLowerDeviceObject(fdo);PIRP irp;PIO_STACK_LOCATION s;KEVENT event;NTSTATUS status;
 if(!lower)return STATUS_NO_SUCH_DEVICE;
 irp=IoAllocateIrp(lower->StackSize,FALSE);if(!irp){ObDereferenceObject(lower);return STATUS_INSUFFICIENT_RESOURCES;}
 KeInitializeEvent(&event,NotificationEvent,FALSE);irp->IoStatus.Status=STATUS_NOT_SUPPORTED;
 s=IoGetNextIrpStackLocation(irp);s->MajorFunction=IRP_MJ_PNP;s->MinorFunction=IRP_MN_QUERY_INTERFACE;
 s->Parameters.QueryInterface.InterfaceType=&MT8167MusbOwnerInterfaceGuid;
 s->Parameters.QueryInterface.Size=sizeof(*owner);s->Parameters.QueryInterface.Version=MT_MUSB_OWNER_VERSION;
 s->Parameters.QueryInterface.Interface=(PINTERFACE)owner;s->Parameters.QueryInterface.InterfaceSpecificData=NULL;
 IoSetCompletionRoutine(irp,QueryDone,&event,TRUE,TRUE,TRUE);
 status=IoCallDriver(lower,irp);
 if(status==STATUS_PENDING)KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);
 status=irp->IoStatus.Status;IoFreeIrp(irp);ObDereferenceObject(lower);return status;
}
#else
#define QueryOwner MtTestQueryOwner
#endif
static int Fresh(const MT_MUSB_OBSERVATION *o,ULONGLONG generation,int powered)
{
 ULONGLONG now=KeQueryInterruptTime();
 return o->bytes==sizeof(*o)&&o->generation&&(!generation||generation==o->generation)&&
  (o->capabilities&MT_MUSB_REQUIRED)==MT_MUSB_REQUIRED&&
  (o->valid&MT_MUSB_REQUIRED)==MT_MUSB_REQUIRED&&o->host_role==1&&o->oc_clear==1&&
  (powered<0||(powered?o->vbus_on==1:(o->devctl&0x18)==0))&&
  (o->evidence&(MT_MUSB_OC_PIN21|MT_MUSB_VBUS_COMPARATOR|MT_MUSB_NO_FORCED_VBUS))==
   (MT_MUSB_OC_PIN21|MT_MUSB_VBUS_COMPARATOR|MT_MUSB_NO_FORCED_VBUS)&&o->observed_100ns<=now&&now-o->observed_100ns<=MT_FRESH;
}
static int Observe(struct mt_musb_runtime *r,int powered)
{
 MT_MUSB_OBSERVATION o;KIRQL irql;RtlZeroMemory(&o,sizeof(o));o.bytes=sizeof(o);
 if(!NT_SUCCESS(r->owner.Observe(r->owner.Header.Context,&o))||!Fresh(&o,r->proof.generation,powered))return -1;
 /* A lease cannot silently move mappings underneath the MMIO owner. */
 if(o.mac.QuadPart!=r->proof.mac.QuadPart||o.phy.QuadPart!=r->proof.phy.QuadPart||
    o.raw_mac.QuadPart!=r->proof.raw_mac.QuadPart||o.raw_phy.QuadPart!=r->proof.raw_phy.QuadPart||
    o.mac_bytes!=MT_WINDOW||o.phy_bytes!=MT_WINDOW)return -1;
 KeAcquireSpinLock(&r->lock,&irql);r->proof=o;KeReleaseSpinLock(&r->lock,irql);return 0;
}
static int EnginesIdle(struct mt_musb_runtime *r)
{
 unsigned n;
 if(Mt8167UsbPioReady(&r->controller))return -1;
 /* Linux mtk_qmu.h: USBGCSR is QMU+0x300, QCSR ACTIVE bit15.
  * Never stop/overwrite an unknown running QMU or firmware DMA channel. */
 if(Rd32(r,0xb00))return -1;
 for(n=0;n<8;n++)if((Rd32(r,0x810+n*16)|Rd32(r,0xa00+n*16))&0x8000u)return -1;
 return 0;
}
static int ReleaseForces(struct mt_musb_runtime *r)
{UCHAR v=PhyRead(r,0x6d);PhyWrite(r,0x6d,(UCHAR)(v&~0x3eu));
 return (Rd8(r,0x74)&4)||(PhyRead(r,0x6d)&0x3e)?-1:0;}
static int IrqOff(struct mt_musb_runtime *r)
{return (Rd32(r,0xa4)||Rd16(r,6)||Rd16(r,8)||Rd8(r,0xb))?-1:0;}
static int FlushLane(struct mt_musb_runtime *r,unsigned n)
{
 struct mt_musb_lane *l=&r->lanes[n];unsigned v;int failed=0;
 Wr8(r,0xe,(UCHAR)n);
 if(!n){Wr16(r,0x12,0x100);Wr16(r,0x12,0x100);if(Rd16(r,0x12)&0x23)failed=1;}
 else {
  Wr16(r,0x12,0x2008);Wr16(r,0x12,0x2008);v=Rd16(r,0x12);
  if(v&3)failed=1;if(l->properties.direction)l->pio.ep.toggle=!!(v&0x100);
  Wr16(r,0x16,0x10);Wr16(r,0x16,0x10);v=Rd16(r,0x16);
  if(v&0x21)failed=1;if(!l->properties.direction)l->pio.ep.toggle=!!(v&0x200);
 }
 l->hardware_job=FALSE;
 return failed||EnginesIdle(r)||IrqOff(r)?-1:0;
}
static int Flush(struct mt_musb_runtime *r)
{unsigned n;int failed=0;for(n=0;n<MT_MUSB_LANES;n++)if(FlushLane(r,n))failed=1;return failed?-1:0;}
static int AllIdle(struct mt_musb_runtime *r)
{unsigned n;for(n=0;n<MT_MUSB_LANES;n++)if(r->lanes[n].job.state!=MT_JOB_IDLE||r->lanes[n].hardware_job)return 0;return 1;}
/* Under runtime lock. A canceled lane may be reused before USBPORT reopens
 * its old endpoint, so recover toggle by cookie independently of FIFO reuse. */
static void RecoverRecord(struct mt_musb_runtime *r,ULONG cookie,ULONG toggle)
{unsigned i;if(!cookie)return;for(i=0;i<32;i++)if(r->recovered[i].cookie==cookie)break;
 if(i==32)i=r->recovery_next++%32;r->recovered[i].cookie=cookie;r->recovered[i].toggle=toggle;}
static int StopPhysical(struct mt_musb_runtime *r)
{
 MT_MUSB_OBSERVATION proof;int local=0;
 if(!r->lease)return 0;
 /* DMA was never enabled by this driver. Only our established PIO ownership
  * permits these mask/flush writes. Parent isolation is the fallback proof. */
 if(r->started&&!r->io_unsafe&&(EnginesIdle(r)||IrqOff(r)))r->io_unsafe=TRUE;
 if(r->started&&!r->io_unsafe){
  Wr32(r,0xa4,Rd32(r,0xa4)&~7u);Wr16(r,6,0);Wr16(r,8,0);Wr8(r,0xb,0);
  local=Flush(r);Wr8(r,0x60,(UCHAR)(Rd8(r,0x60)&~1u));
 }
 RtlZeroMemory(&proof,sizeof(proof));proof.bytes=sizeof(proof);
 if(!NT_SUCCESS(r->owner.StopAndProve(r->owner.Header.Context,&proof)))return -1;
 /* A hardware isolation proof may succeed even after local readback failed.
  * It must prove engines+IRQ stopped and our GPIO source physically deasserted,
  * same lease. External PC-supplied voltage is separately reported. */
 if(proof.bytes!=sizeof(proof)||proof.generation!=r->proof.generation||
    proof.observed_100ns>KeQueryInterruptTime()||KeQueryInterruptTime()-proof.observed_100ns>MT_FRESH||
    (proof.valid&MT_MUSB_STOP_PROOF)!=MT_MUSB_STOP_PROOF||
    !proof.source_off||!(proof.evidence&MT_MUSB_SOURCE_OFF))return -1;
 UNREFERENCED_PARAMETER(local);
 if(!NT_SUCCESS(r->owner.Release(r->owner.Header.Context)))return -1;
 r->lease=FALSE;r->started=FALSE;
 {KIRQL irql;KeAcquireSpinLock(&r->lock,&irql);r->proof=proof;
  RtlZeroMemory(&r->observation,sizeof(r->observation));
  r->observation.vbus_off_verified=(proof.valid&MT_MUSB_VBUS)&&!(proof.devctl&0x18);r->observation.endpoints_quiesced=1;r->observation.source_off_verified=1;
  KeReleaseSpinLock(&r->lock,irql);}
 return 0;
}
static void Destroy(struct mt_musb_runtime *r)
{
 if(r->phy)MmUnmapIoSpace((PVOID)r->phy,MT_WINDOW);
 if(r->mac)MmUnmapIoSpace((PVOID)r->mac,MT_WINDOW);
 if(r->owner.Header.InterfaceDereference)r->owner.Header.InterfaceDereference(r->owner.Header.Context);
 if(r->work)IoFreeWorkItem(r->work);if(r->cleanup)IoFreeWorkItem(r->cleanup);
 ObDereferenceObject(r->fdo);ExFreePoolWithTag(r,MT_TAG);
}
static void Tick(PDEVICE_OBJECT,PVOID);
static void QueueLocked(struct mt_musb_runtime *r)
{
 if(!r->closing&&!r->queued){r->queued=TRUE;KeClearEvent(&r->idle);IoQueueWorkItem(r->work,Tick,DelayedWorkQueue,r);}
}
static void TimerDpc(PKDPC dpc,PVOID context,PVOID a,PVOID b)
{
 struct mt_musb_runtime *r=context;KIRQL irql;UNREFERENCED_PARAMETER(dpc);UNREFERENCED_PARAMETER(a);UNREFERENCED_PARAMETER(b);
 KeAcquireSpinLock(&r->lock,&irql);QueueLocked(r);KeReleaseSpinLock(&r->lock,irql);
}
static int Begin(struct mt_musb_runtime *r,unsigned n)
{
 struct mt_musb_lane *l=&r->lanes[n];struct a04_mp_properties *e=&l->properties;struct a04_pio_endpoint p;unsigned interval;
 if(!a04_usbport_native_route(e->hub)||a04_usbport_interval(e->speed,e->type,e->period,&interval))return -1;
 if(e->type==1){
  Wr8(r,0xe,0);Wr16(r,0x480,e->address);Wr16(r,0x482,0);
  Wr8(r,0x1a,(UCHAR)(e->speed==2?0x40:e->speed==1?0x80:0xc0));Wr8(r,0x1b,0);Wr16(r,2,1);
  return Mt8167Ep0Start(&l->ep0,&r->controller.io,l->parameters.setup,l->shadow,l->parameters.bytes,e->packet)==EP0_SEND_SETUP?0:-1;
 }
 p.address=e->address;p.endpoint=e->endpoint&15;p.packet=e->packet;
 p.speed=e->speed==2?0x40:e->speed==1?0x80:0xc0;p.type=e->type;p.direction=e->direction;
 p.period=interval;p.toggle=l->job.toggle;
 return a04_pio_configure_lane(&r->controller.io,&l->pio,&p,l->shadow,l->parameters.bytes,n);
}
static int DataEvent(struct mt_musb_runtime *r,unsigned n)
{
 struct mt_musb_lane *l=&r->lanes[n];unsigned source;
 if(l->properties.type==1){
  EP0_ACTION a;if(!(Rd16(r,2)&1))return 0;Wr16(r,2,1);a=Mt8167Ep0Interrupt(&l->ep0);
  return a==EP0_COMPLETE?1:a==EP0_ABORT?-1:0;
 }
 source=l->properties.direction?2:4;if(!(Rd16(r,source)&(1u<<n)))return 0;
 Wr16(r,source,(USHORT)(1u<<n));return a04_pio_event(&r->controller.io,&l->pio);
}
static int RootAction(struct mt_musb_runtime *r,ULONG kind,struct a04_hub_observation *o)
{
 ULONGLONG start;ULONG power;
 RtlZeroMemory(o,sizeof(*o));
 if(!AllIdle(r)||Flush(r))return -1;
 if(kind==A04_HUB_POWER_OFF){
  Wr8(r,0x60,(UCHAR)(Rd8(r,0x60)&~1u));
  {MT_MUSB_OBSERVATION proof;KIRQL irql;
   if(Vbus(r,0))return -1;
   RtlZeroMemory(&proof,sizeof(proof));proof.bytes=sizeof(proof);
   if(!NT_SUCCESS(r->owner.Observe(r->owner.Header.Context,&proof))||proof.bytes!=sizeof(proof)||
      proof.generation!=r->proof.generation||!proof.source_off||!(proof.evidence&MT_MUSB_SOURCE_OFF)||
      proof.observed_100ns>KeQueryInterruptTime()||KeQueryInterruptTime()-proof.observed_100ns>MT_FRESH)return -1;
   KeAcquireSpinLock(&r->lock,&irql);r->proof=proof;KeReleaseSpinLock(&r->lock,irql);
   o->source_off_verified=1;o->vbus_off_verified=(proof.valid&MT_MUSB_VBUS)&&!(proof.devctl&0x18);
   o->endpoints_quiesced=1;return 0;
  }
 }
 if(kind==A04_HUB_DISABLE){
  /* MUSB has no independent ENABLE latch. Linux musb_virthub clears the
   * logical bit. First prove FIFO/REQ idle and health; keep SESSION/VBUS. */
  if(Observe(r,1))return -1;
 }else if(kind==A04_HUB_POWER_ON){if(A04UsbHostRun(&r->board)||ReleaseForces(r)||Observe(r,1))return -1;}
 else if(kind==A04_HUB_RESET){
  if(Observe(r,1))return -1;
  power=Rd8(r,1);Wr8(r,1,(UCHAR)((power&~4u)|8u));if(!(Rd8(r,1)&8))return -1;
  start=KeQueryInterruptTime();Delay(r,50);o->reset_asserted_ms=(unsigned)((KeQueryInterruptTime()-start)/10000);
  Wr8(r,1,(UCHAR)(Rd8(r,1)&~8u));if(Rd8(r,1)&8)return -1;
  Delay(r,10);if(Observe(r,1))return -1;
  o->speed=(Rd8(r,1)&0x10)?A04_PORT_HIGH_SPEED:(Rd8(r,0x60)&0x20)?A04_PORT_LOW_SPEED:0;
 }else return -1;
 o->qualified=o->id_host=o->overcurrent_clear=o->vbus_verified=o->endpoints_quiesced=1;return 0;
}
/* Service one resident hardware endpoint. No wait/flush on PENDING/NAK:
 * other hardware FIFOs can progress without losing an in-flight packet. */
static int ServiceLane(struct mt_musb_runtime *r,unsigned n)
{
 struct mt_musb_lane *l=&r->lanes[n];KIRQL k;ULONG state,ticket,actual=0,toggle=0;int claim=0,result=0,proven=1;
 KeAcquireSpinLock(&r->lock,&k);state=l->job.state;ticket=l->job.ticket;
 if(state==MT_JOB_QUEUED){claim=!mt_musb_claim(&l->job,&ticket);if(mt_musb_expired(&l->job,KeQueryInterruptTime()))result=-1;}
 else if(state==MT_JOB_CANCEL&&!l->hardware_job){
  (void)mt_musb_finish(&l->job,ticket,0,USBD_STATUS_CANCELED,l->job.toggle,1);
  RecoverRecord(r,l->job.last_cookie,l->job.last_toggle);
 }
 KeReleaseSpinLock(&r->lock,k);
 if(claim&&!result){
  KeAcquireSpinLock(&r->lock,&k);
  if(r->closing||!r->connected||l->job.state!=MT_JOB_RUNNING||mt_musb_expired(&l->job,KeQueryInterruptTime()))result=-1;
  else {
   RtlZeroMemory(&l->ep0,sizeof(l->ep0));RtlZeroMemory(&l->pio,sizeof(l->pio));
   l->hardware_job=TRUE;if(Begin(r,n)){result=-1;proven=FlushLane(r,n)==0;}
  }
  KeReleaseSpinLock(&r->lock,k);
 }
 KeAcquireSpinLock(&r->lock,&k);state=l->job.state;
 if(mt_musb_expired(&l->job,KeQueryInterruptTime()))result=-1;
 if(l->properties.type==3&&state==MT_JOB_RUNNING&&!result)l->job.deadline=KeQueryInterruptTime()+50000000ull;
 KeReleaseSpinLock(&r->lock,k);
 if(l->hardware_job){
  if(state==MT_JOB_CANCEL||result){result=-1;proven=FlushLane(r,n)==0;}
  else result=DataEvent(r,n);
  actual=!n?l->ep0.transfer.actual:l->pio.actual;toggle=!n?0:l->pio.ep.toggle;
  if(result){if(FlushLane(r,n))proven=0;else if(n)toggle=l->pio.ep.toggle;}
 }
 KeAcquireSpinLock(&r->lock,&k);
 if(result&&(l->job.state==MT_JOB_RUNNING||l->job.state==MT_JOB_CANCEL)){
  (void)mt_musb_finish(&l->job,ticket,actual,result==1?USBD_STATUS_SUCCESS:
   result==A04_PIO_STALL?USBD_STATUS_STALL_PID:USBD_STATUS_XACT_ERROR,toggle,proven);
  if(proven)RecoverRecord(r,l->job.last_cookie,l->job.last_toggle);
 }
 KeReleaseSpinLock(&r->lock,k);return proven?0:-1;
}
static void Tick(PDEVICE_OBJECT device,PVOID context)
{
 struct mt_musb_runtime *r=context;KIRQL k;ULONG root=0,n,start,usb,connected,frame;BOOLEAN closing,fault;
 struct a04_hub_observation o;LARGE_INTEGER due;int root_result=0,proven=1;
 UNREFERENCED_PARAMETER(device);RtlZeroMemory(&o,sizeof(o));
 KeAcquireSpinLock(&r->lock,&k);closing=r->closing;fault=r->fault;connected=r->connected;frame=r->frame;
 if(!closing&&!fault){root=r->root_pending;r->root_pending=0;if(root)r->root_running=TRUE;}
 start=r->cursor;r->cursor=(start+1)%MT_MUSB_LANES;KeReleaseSpinLock(&r->lock,k);
 if(!closing&&!fault){
  if(EnginesIdle(r)||IrqOff(r)){r->io_unsafe=TRUE;fault=TRUE;}
  else if(Observe(r,r->proof.gpio22?1:-1))fault=TRUE;
  else {
   usb=Rd8(r,0xa)&0xb0;if(usb)Wr8(r,0xa,(UCHAR)usb);
   if(usb&0x10)connected=1;if(usb&0x20)connected=0;
   if(usb&0x80)fault=TRUE;
   if(!connected&&!AllIdle(r))fault=TRUE;
  }
  KeAcquireSpinLock(&r->lock,&k);r->connected=connected;KeReleaseSpinLock(&r->lock,k);
  if(!fault&&root){root_result=RootAction(r,root,&o);if(root_result)fault=TRUE;else if(root==A04_HUB_POWER_OFF)connected=0;}
  if(!fault)for(n=0;n<MT_MUSB_LANES;n++){
   if(KeQueryInterruptTime()-r->proof.observed_100ns>MT_FRESH/2&&Observe(r,r->proof.gpio22?1:-1)){fault=TRUE;break;}
   if(ServiceLane(r,(start+n)%MT_MUSB_LANES)){fault=TRUE;break;}
  }
  frame=Rd16(r,0xc);
  if(fault){
   KeAcquireSpinLock(&r->lock,&k);r->fault=TRUE;KeReleaseSpinLock(&r->lock,k);
   connected=0;proven=StopPhysical(r)==0;
   KeAcquireSpinLock(&r->lock,&k);
   for(n=0;n<MT_MUSB_LANES;n++){
    struct mt_musb_lane *l=&r->lanes[n];ULONG ticket=l->job.ticket;
    if(l->job.state==MT_JOB_QUEUED)(void)mt_musb_claim(&l->job,&ticket);
    if(proven&&l->job.state==MT_JOB_FAULT)l->job.state=MT_JOB_RUNNING; /* independent isolation now proved */
    if(l->job.state==MT_JOB_RUNNING||l->job.state==MT_JOB_CANCEL){
     (void)mt_musb_finish(&l->job,ticket,l->job.actual,USBD_STATUS_XACT_ERROR,l->job.toggle,proven);
     if(proven)RecoverRecord(r,l->job.last_cookie,l->job.last_toggle);
    }
   }
   KeReleaseSpinLock(&r->lock,k);
  }
 }
 KeAcquireSpinLock(&r->lock,&k);r->fault=fault;r->connected=connected;r->frame=frame;
 if(root){r->root_running=FALSE;r->root_done=root;r->root_result=(root_result||fault)?1:0;if(!fault)r->observation=o;}
 r->queued=FALSE;if(!r->closing&&!r->fault){due.QuadPart=-10000;KeSetTimer(&r->timer,due,&r->dpc);}
 KeSetEvent(&r->idle,IO_NO_INCREMENT,FALSE);KeReleaseSpinLock(&r->lock,k);
}
static BOOLEAN ClosePassive(struct mt_musb_runtime *r)
{
 /* Closing was published before timer cancellation, so even an already
  * executing DPC cannot queue work after this drain. Worker never waits on us. */
 KeCancelTimer(&r->timer);KeRemoveQueueDpc(&r->dpc);KeFlushQueuedDpcs();
 KeWaitForSingleObject(&r->idle,Executive,KernelMode,FALSE,NULL);
 {KIRQL irql;KeAcquireSpinLock(&r->lock,&irql);KeReleaseSpinLock(&r->lock,irql);}
 if(StopPhysical(r)){
  r->fault=TRUE;
  DbgPrint("MT8167MUSBHcd FAULT_RETAINED: stop proof unavailable; lease, mappings and shadow retained\n");
  return FALSE;
 }
 Destroy(r);return TRUE;
}
static void Cleanup(PDEVICE_OBJECT device,PVOID context)
{UNREFERENCED_PARAMETER(device);(void)ClosePassive(context);}
BOOLEAN MtMusbClose(struct mt_musb_runtime *r)
{
 KIRQL irql;if(!r)return TRUE;
 KeAcquireSpinLock(&r->lock,&irql);r->closing=TRUE;{unsigned n;for(n=0;n<MT_MUSB_LANES;n++)mt_musb_cancel(&r->lanes[n].job,r->lanes[n].job.cookie);}KeReleaseSpinLock(&r->lock,irql);
 if(KeGetCurrentIrql()==PASSIVE_LEVEL)return ClosePassive(r);
 IoQueueWorkItem(r->cleanup,Cleanup,DelayedWorkQueue,r);return FALSE;
}
NTSTATUS MtMusbOpen(PDEVICE_OBJECT fdo,struct mt_musb_runtime **out)
{
 struct mt_musb_runtime *r;NTSTATUS status;uint64_t mac,phy;struct a04_usb_memory_pair pairs[2];
 MT_MUSB_OBSERVATION o;MT8167_USB20_IO *io;KIRQL irql;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!fdo||!out)return STATUS_INVALID_DEVICE_STATE;
 *out=NULL;r=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*r),MT_TAG);if(!r)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(r,sizeof(*r));r->fdo=fdo;ObReferenceObject(fdo);
 KeInitializeSpinLock(&r->lock);KeInitializeEvent(&r->idle,NotificationEvent,TRUE);
 KeInitializeTimer(&r->timer);KeInitializeDpc(&r->dpc,TimerDpc,r);
 status=QueryOwner(fdo,&r->owner);if(!NT_SUCCESS(status)){RtlZeroMemory(&r->owner,sizeof(r->owner));goto fail;}
 if(r->owner.Header.Size!=sizeof(r->owner)||r->owner.Header.Version!=MT_MUSB_OWNER_VERSION||
  !r->owner.Header.InterfaceReference||!r->owner.Header.InterfaceDereference||!r->owner.Query||!r->owner.Acquire||
  !r->owner.SetVbus||!r->owner.Observe||!r->owner.StopAndProve||!r->owner.Release){status=STATUS_NOT_SUPPORTED;goto fail;}
 RtlZeroMemory(&o,sizeof(o));o.bytes=sizeof(o);status=r->owner.Query(r->owner.Header.Context,&o);
 /* Qualification MUST precede Acquire, map, PHY, DMA/IRQ, GPIO and VBUS.
  * Current parent v1 (ID/source health only) cannot enter this path. */
 if(!NT_SUCCESS(status)||o.bytes!=sizeof(o)||!o.generation||
    (o.evidence&MT_MUSB_QUERY_READY)==0||o.observed_100ns>KeQueryInterruptTime()||
    KeQueryInterruptTime()-o.observed_100ns>MT_FRESH||
    (o.valid&(MT_MUSB_ROLE|MT_MUSB_EXCLUSIVE|MT_MUSB_RESOURCE_ASSIGNED|MT_MUSB_STOP_CAPABLE))!=
     (MT_MUSB_ROLE|MT_MUSB_EXCLUSIVE|MT_MUSB_RESOURCE_ASSIGNED|MT_MUSB_STOP_CAPABLE)||!o.host_role){status=STATUS_NOT_SUPPORTED;goto fail;}
 pairs[0].raw=o.raw_mac.QuadPart;pairs[0].translated=o.mac.QuadPart;pairs[0].length=o.mac_bytes;
 pairs[1].raw=o.raw_phy.QuadPart;pairs[1].translated=o.phy.QuadPart;pairs[1].length=o.phy_bytes;
 if(a04_usb_translate_memory(pairs,&mac,&phy)){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
 r->proof=o;r->work=IoAllocateWorkItem(fdo);r->cleanup=IoAllocateWorkItem(fdo);
 if(!r->work||!r->cleanup){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
 status=r->owner.Acquire(r->owner.Header.Context,o.generation);if(!NT_SUCCESS(status))goto fail;r->lease=TRUE;
 if(Observe(r,0)||(r->proof.valid&MT_MUSB_STOP_PROOF)!=MT_MUSB_STOP_PROOF){status=STATUS_DEVICE_POWER_FAILURE;goto fail;}
 r->mac=MmMapIoSpace(o.mac,MT_WINDOW,MmNonCached);r->phy=MmMapIoSpace(o.phy,MT_WINDOW,MmNonCached);
 if(!r->mac||!r->phy){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
 io=&r->controller.io;io->context=r;io->read8=Rd8;io->read16=Rd16;io->write8=Wr8;io->write16=Wr16;
 r->controller.read32=Rd32;r->controller.write32=Wr32;
 if(EnginesIdle(r)||IrqOff(r)){status=STATUS_DEVICE_BUSY;goto fail;}
 r->started=TRUE; /* exclusive known-idle PIO ownership, cleanup may now flush */
 r->board.context=r;r->board.power=BoardPower;r->board.vbus=Vbus;
 r->board.phy_read=PhyRead;r->board.phy_write=PhyWrite;r->board.phy_read32=PhyRead32;
 r->board.mac_read=Rd8;r->board.mac_write=Wr8;r->board.delay_ms=Delay;
 if(A04UsbHostPrepare(&r->board)||ReleaseForces(r)||Observe(r,0)||Flush(r)){status=STATUS_DEVICE_HARDWARE_ERROR;goto fail;}
 /* Prepare only. Root POWER command performs Run and publishes real power
  * proof asynchronously; root-only software Start is never returned. */
 KeAcquireSpinLock(&r->lock,&irql);QueueLocked(r);KeReleaseSpinLock(&r->lock,irql);
 *out=r;return STATUS_SUCCESS;
fail:
 if(StopPhysical(r)){
  r->fault=TRUE;DbgPrint("MT8167MUSBHcd admission failure with unproved shutdown: FAULT_RETAINED\n");
  return status;
 }
 Destroy(r);return status;
}
ULONG MtMusbSubmitDeadline(struct mt_musb_runtime *r,ULONG cookie,const struct a04_mp_properties *e,ULONG toggle,
 const struct a04_mp_parameters *p,PVOID buffer,ULONGLONG deadline)
{
 KIRQL k;ULONG result=1,n,first,last;ULONGLONG now=KeQueryInterruptTime(),span=50000000ull;unsigned interval;
 if(!r||!p||!e||!cookie||!a04_usbport_native_route(e->hub)||
  a04_usbport_interval(e->speed,e->type,e->period,&interval)||p->bytes>MT_MUSB_SHADOW_BYTES||(p->bytes&&!buffer))return 1;
 if(deadline){if(deadline<=now)return 1;span=deadline-now;if(span>50000000ull)return 1;}
 first=e->type==1?0:1;last=e->type==1?1:MT_MUSB_LANES;
 KeAcquireSpinLock(&r->lock,&k);
 if(!r->closing&&!r->fault&&!r->root_pending&&!r->root_running&&!r->root_done&&r->proof.vbus_on&&r->connected){
  for(n=0;n<MT_MUSB_LANES;n++)if(r->lanes[n].job.state!=MT_JOB_IDLE){
   struct mt_musb_lane *old=&r->lanes[n];
   if(old->job.cookie==cookie||(old->properties.address==e->address&&old->properties.endpoint==e->endpoint&&
     old->properties.type==e->type&&old->properties.direction==e->direction))goto out;
  }
  for(n=first;n<last;n++){
   struct mt_musb_lane *l=&r->lanes[n];
   if(l->hardware_job||mt_musb_admit(&l->job,cookie,now,span))continue;
   l->properties=*e;l->properties.common_va=NULL;l->properties.common_pa=0;l->properties.common_bytes=0;l->parameters=*p;l->job.toggle=toggle;RtlZeroMemory(l->shadow,p->bytes);
   if(p->bytes&&(e->type==1?!(p->setup[0]&0x80):e->direction!=0))RtlCopyMemory(l->shadow,buffer,p->bytes);
   QueueLocked(r);result=0;break;
  }
 }
out:KeReleaseSpinLock(&r->lock,k);return result;
}
ULONG MtMusbSubmit(struct mt_musb_runtime *r,ULONG cookie,const struct a04_mp_properties *e,ULONG toggle,
 const struct a04_mp_parameters *p,PVOID buffer)
{return MtMusbSubmitDeadline(r,cookie,e,toggle,p,buffer,0);}
VOID MtMusbCancel(struct mt_musb_runtime *r,ULONG cookie)
{
 KIRQL k;unsigned n;if(!r)return;KeAcquireSpinLock(&r->lock,&k);
 for(n=0;n<MT_MUSB_LANES;n++){
  struct mt_musb_lane *l=&r->lanes[n];
  if(l->job.cookie!=cookie)continue;
  if(l->job.state==MT_JOB_DONE)RecoverRecord(r,l->job.cookie,l->job.toggle);
  mt_musb_cancel(&l->job,cookie);
 }
 QueueLocked(r);KeReleaseSpinLock(&r->lock,k);
}
ULONG MtMusbTake(struct mt_musb_runtime *r,ULONG cookie,PVOID buffer,ULONG bytes,PULONG actual,PULONG status,PULONG toggle)
{
 KIRQL k;unsigned n;ULONG result=1;if(!r)return 1;KeAcquireSpinLock(&r->lock,&k);
 for(n=0;n<MT_MUSB_LANES;n++){
  struct mt_musb_lane *l=&r->lanes[n];
  if(l->job.state==MT_JOB_DONE&&l->job.cookie==cookie&&l->job.actual<=bytes&&(!l->job.actual||buffer)&&
    !mt_musb_take(&l->job,cookie,actual,status,toggle)){
   if(*actual&&(l->properties.type==1?(l->parameters.setup[0]&0x80):!l->properties.direction))RtlCopyMemory(buffer,l->shadow,*actual);
   result=0;break;
  }
 }
 KeReleaseSpinLock(&r->lock,k);return result;
}
ULONG MtMusbRecoverToggle(struct mt_musb_runtime *r,ULONG cookie,PULONG toggle)
{
 KIRQL k;ULONG result=1;unsigned n;if(!r)return 1;KeAcquireSpinLock(&r->lock,&k);
 for(n=0;n<MT_MUSB_LANES;n++)if(r->lanes[n].job.cookie==cookie&&r->lanes[n].job.state!=MT_JOB_IDLE)goto out;
 if(!r->fault)for(n=0;n<32;n++)if(r->recovered[n].cookie==cookie){*toggle=r->recovered[n].toggle;result=0;break;}
out:KeReleaseSpinLock(&r->lock,k);return result;
}
ULONG MtMusbRootQueue(struct mt_musb_runtime *r,ULONG kind)
{
 KIRQL irql;ULONG result=1;if(!r||kind<A04_HUB_POWER_ON||kind>A04_HUB_DISABLE)return 1;KeAcquireSpinLock(&r->lock,&irql);
 if(!r->closing&&!r->fault&&!r->root_pending&&!r->root_running&&!r->root_done&&AllIdle(r)){r->root_pending=kind;QueueLocked(r);result=0;}
 KeReleaseSpinLock(&r->lock,irql);return result;
}
ULONG MtMusbObserve(struct mt_musb_runtime *r,struct a04_hub_observation *o,PULONG connected,PULONG done,PULONG result)
{
 KIRQL irql;ULONG fault;if(!r)return 1;KeAcquireSpinLock(&r->lock,&irql);
 *o=r->observation;*connected=r->connected;*done=r->root_done;*result=r->root_result;r->root_done=0;
 fault=r->fault;KeReleaseSpinLock(&r->lock,irql);return fault;
}
ULONG MtMusbFrame(struct mt_musb_runtime *r)
{KIRQL irql;ULONG n=0;if(r){KeAcquireSpinLock(&r->lock,&irql);n=r->frame;KeReleaseSpinLock(&r->lock,irql);}return n;}
