/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "capture_owner.h"
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
#include "ul_engine.h"
#include "wave_dma_live.h"
#include "wavert_dma.h"
#define CAP_TAG 'cAuM'
struct Mt8167CaptureOwner {
 volatile LONG refs;KMUTEX lock;EX_RUNDOWN_REF positions;Mt8167CaptureResources r;Mt8167StreamBackend api;
 Mt8167WaveBuffer buffer;Mt8167WaveDma dma;mt8167_ul_engine engine;
 PKINTERRUPT interrupt;PPORTWAVERTSTREAM client;BOOLEAN leased,closed;
 NTSTATUS failure;KSSTATE state;ULONG position;
};
static int Read(void *c,uint32_t off,uint32_t *v)
{Mt8167CaptureOwner *o=(Mt8167CaptureOwner *)c;if(!o->leased||!v||(off&3)||off>=0x1000)return -1;*v=READ_REGISTER_ULONG((volatile ULONG *)((PUCHAR)o->r.afe+off));return 0;}
static int Write(void *c,uint32_t off,uint32_t v)
{Mt8167CaptureOwner *o=(Mt8167CaptureOwner *)c;if(!o->leased)return -1;
 switch(off){case 0x10:case 0x14:case 0x2c:case 0x6c:case 0x80:case 0x88:case 0x114:case 0x120:case 0x124:case 0x13c:case 0x3a0:case 0x3a8:case 0x3b0:case 0x3b4:case 0x3d8:break;default:return -1;}
 WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)o->r.afe+off),v);return 0;}
static void Reference(void *c){InterlockedIncrement(&((Mt8167CaptureOwner *)c)->refs);}
static void Dereference(void *c)
{
 Mt8167CaptureOwner *o=(Mt8167CaptureOwner *)c;if(InterlockedDecrement(&o->refs))return;
 /* Never free an ISR context, an exposed ring, or a provider needed by DMA. */
 if(o->failure||o->dma.exposed||o->engine.exposed||o->interrupt||o->client||KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
 if(o->dma.list&&!NT_SUCCESS(Mt8167WaveDmaRelease(&o->dma,FALSE)))return;
 if(o->buffer.mdl&&!NT_SUCCESS(Mt8167ReleaseUnpublishedBuffer(&o->buffer)))return;
 if(o->leased&&!NT_SUCCESS(o->r.releaseUnexposed(o->r.context)))return;
 o->r.dereference(o->r.context);ExFreePoolWithTag(o,CAP_TAG);
}
static BOOLEAN Isr(PKINTERRUPT,PVOID c)
{
 Mt8167CaptureOwner *o=(Mt8167CaptureOwner *)c;uint32_t notify;
 int rc=mt8167_ul_interrupt(&o->engine,&notify);if(rc<0)o->failure=STATUS_DEVICE_HARDWARE_ERROR;
 /* WaveRT polling reads real CUR; no fabricated event period/position. */
 return rc>0;
}
static BOOLEAN StartSynchronized(PVOID c)
{Mt8167CaptureOwner *o=(Mt8167CaptureOwner *)c;if(mt8167_ul_start(&o->engine))o->failure=STATUS_DEVICE_HARDWARE_ERROR;return TRUE;}
static BOOLEAN StopSynchronized(PVOID c)
{Mt8167CaptureOwner *o=(Mt8167CaptureOwner *)c;if(mt8167_ul_stop(&o->engine))o->failure=STATUS_DEVICE_HARDWARE_ERROR;return TRUE;}
static BOOLEAN PositionSynchronized(PVOID c)
{Mt8167CaptureOwner *o=(Mt8167CaptureOwner *)c;uint32_t p;if(mt8167_ul_position(&o->engine,&p))o->failure=STATUS_DEVICE_HARDWARE_ERROR;else o->position=p;return TRUE;}
static NTSTATUS Checkout(void *c,PPORTWAVERTSTREAM port,ULONG bytes,PMDL *mdl,PULONG actual)
{
 Mt8167CaptureOwner *o=(Mt8167CaptureOwner *)c;if(!port||!mdl||!actual||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 *mdl=NULL;*actual=0;KeWaitForSingleObject(&o->lock,Executive,KernelMode,FALSE,NULL);NTSTATUS s=STATUS_SUCCESS;
 if(o->closed||o->failure)s=STATUS_DEVICE_NOT_READY;
 else if(o->client)s=STATUS_DEVICE_BUSY;
 else if(o->buffer.mdl){if(!bytes||bytes>o->buffer.bytes)s=STATUS_BUFFER_TOO_SMALL;}
 else {
  if(bytes<8192||bytes>65536)s=STATUS_INVALID_BUFFER_SIZE;
  if(NT_SUCCESS(s))s=Mt8167AllocateBuffer(port,bytes,&o->buffer);
  if(NT_SUCCESS(s))s=Mt8167WaveDmaMapCapture(o->r.adapter,o->r.pdo,o->buffer.mdl,o->buffer.bytes,&o->dma);
  /* A timeout may leave a pending callback owning the adapter/MDL. Retain
   * rather than letting caller destroy or reuse a possibly mapped buffer. */
  if(!NT_SUCCESS(s))o->failure=s;
 }
 if(NT_SUCCESS(s)){o->client=port;port->AddRef();*mdl=o->buffer.mdl;*actual=o->buffer.bytes;}
 KeReleaseMutex(&o->lock,FALSE);return s;
}
static NTSTATUS Checkin(void *c,PPORTWAVERTSTREAM port,PMDL mdl)
{
 Mt8167CaptureOwner *o=(Mt8167CaptureOwner *)c;if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 KeWaitForSingleObject(&o->lock,Executive,KernelMode,FALSE,NULL);NTSTATUS s=STATUS_SUCCESS;
 if(o->client!=port||o->buffer.mdl!=mdl)s=STATUS_INVALID_PARAMETER;
 else if(o->engine.running||o->failure)s=STATUS_DEVICE_BUSY;
 else {o->client=NULL;port->Release();}
 KeReleaseMutex(&o->lock,FALSE);return s;
}
static NTSTATUS SetState(void *c,PPORTWAVERTSTREAM port,KSSTATE state)
{
 Mt8167CaptureOwner *o=(Mt8167CaptureOwner *)c;if(KeGetCurrentIrql()!=PASSIVE_LEVEL||state>KSSTATE_RUN)return STATUS_INVALID_PARAMETER;
 KeWaitForSingleObject(&o->lock,Executive,KernelMode,FALSE,NULL);NTSTATUS s=STATUS_SUCCESS;
 if(o->closed||o->failure||o->client!=port||!o->dma.list)s=STATUS_DEVICE_NOT_READY;
 else if(state==KSSTATE_RUN&&!o->engine.running){
  if(!o->engine.prepared){
   o->dma.exposed=TRUE;o->buffer.handedToDevice=TRUE;
   if(mt8167_ul_prepare(&o->engine,(uint64_t)o->dma.logical.QuadPart,o->dma.bytes,o->dma.bytes/2))s=STATUS_DEVICE_HARDWARE_ERROR;
   if(NT_SUCCESS(s))s=IoConnectInterrupt(&o->interrupt,Isr,o,NULL,o->r.vector,o->r.level,o->r.level,o->r.mode,TRUE,o->r.affinity,FALSE);
  }
  if(NT_SUCCESS(s))KeSynchronizeExecution(o->interrupt,StartSynchronized,o);
 }else if(state!=KSSTATE_RUN&&o->engine.running)KeSynchronizeExecution(o->interrupt,StopSynchronized,o);
 if(!NT_SUCCESS(s))o->failure=s;if(o->failure)s=o->failure;
 if(NT_SUCCESS(s))o->state=state;
 KeReleaseMutex(&o->lock,FALSE);return s;
}
static NTSTATUS Position(void *c,PKSAUDIO_POSITION p)
{
 Mt8167CaptureOwner *o=(Mt8167CaptureOwner *)c;if(!p)return STATUS_INVALID_PARAMETER;
 if(!ExAcquireRundownProtection(&o->positions))return STATUS_DEVICE_NOT_READY;
 NTSTATUS s=STATUS_DEVICE_NOT_READY;
 if(!o->closed&&!o->failure&&o->interrupt&&o->engine.running){
  KeSynchronizeExecution(o->interrupt,PositionSynchronized,o);s=o->failure;
  if(NT_SUCCESS(s)){KeMemoryBarrier();p->PlayOffset=o->position;p->WriteOffset=o->position;}
 }
 ExReleaseRundownProtection(&o->positions);return s;
}
static NTSTATUS Description(void *,PDEVICE_DESCRIPTION d)
{if(!d)return STATUS_INVALID_PARAMETER;RtlZeroMemory(d,sizeof(*d));d->Version=DEVICE_DESCRIPTION_VERSION3;d->Master=TRUE;d->ScatterGather=FALSE;d->DmaAddressWidth=32;d->InterfaceType=Internal;d->MaximumLength=65536;return STATUS_SUCCESS;}
/* Engine PCM exclusively uses checkout/checkin/setState. Legacy entrypoints
 * are not simulated hardware operations. */
static NTSTATUS Allocate(void *,ULONG,PMDL *,PULONG,PULONG,MEMORY_CACHING_TYPE *){return STATUS_NOT_SUPPORTED;}
static void Free(void *,PMDL,ULONG){}
static NTSTATUS Start(void *,BOOLEAN *){return STATUS_NOT_SUPPORTED;}
static NTSTATUS Stop(void *){return STATUS_NOT_SUPPORTED;}
NTSTATUS Mt8167CaptureCreate(const Mt8167CaptureResources *r,Mt8167CaptureOwner **out)
{
 if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!r||!r->pdo||!r->afe||!r->adapter||!r->vector||!r->affinity||
    !r->reference||!r->dereference||!r->queryLatency||!r->acquireInput||!r->releaseUnexposed)return STATUS_INVALID_PARAMETER;
 Mt8167CaptureOwner *o=(Mt8167CaptureOwner *)ExAllocatePoolWithTag(NonPagedPool,sizeof(*o),CAP_TAG);
 if(!o)return STATUS_INSUFFICIENT_RESOURCES;RtlZeroMemory(o,sizeof(*o));o->r=*r;o->refs=1;KeInitializeMutex(&o->lock,0);ExInitializeRundownProtection(&o->positions);r->reference(r->context);
 NTSTATUS s=r->queryLatency(r->context,&o->api.captureLatency);
 if(!NT_SUCCESS(s)){r->dereference(r->context);ExFreePoolWithTag(o,CAP_TAG);return s;}
 s=r->acquireInput(r->context,48000);
 /* Failed acquisition may have changed analog hardware; retain provider. */
 if(!NT_SUCCESS(s)){o->failure=s;return s;}o->leased=TRUE;
 o->engine.io.context=o;o->engine.io.read=Read;o->engine.io.write=Write;
 o->api.capture=TRUE;o->api.context=o;o->api.reference=Reference;o->api.dereference=Dereference;
 o->api.allocate=Allocate;o->api.freeBuffer=Free;o->api.start=Start;o->api.stop=Stop;
 o->api.position=Position;o->api.getDeviceDescription=Description;o->api.checkout=Checkout;o->api.checkin=Checkin;o->api.setState=SetState;
 *out=o;return STATUS_SUCCESS;
}
const Mt8167StreamBackend *Mt8167CaptureBackend(Mt8167CaptureOwner *o){return o&&!o->closed&&!o->failure?&o->api:NULL;}
NTSTATUS Mt8167CaptureRevoke(Mt8167CaptureOwner *o)
{
 if(!o||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 KeWaitForSingleObject(&o->lock,Executive,KernelMode,FALSE,NULL);o->closed=TRUE;KeReleaseMutex(&o->lock,FALSE);
 ExWaitForRundownProtectionRelease(&o->positions);KeWaitForSingleObject(&o->lock,Executive,KernelMode,FALSE,NULL);
 if(o->interrupt){KeSynchronizeExecution(o->interrupt,StopSynchronized,o);
  if(!o->failure){IoDisconnectInterrupt(o->interrupt);o->interrupt=NULL;}
 }
 NTSTATUS s=o->failure?o->failure:(o->engine.exposed?STATUS_DEVICE_BUSY:STATUS_SUCCESS);
 KeReleaseMutex(&o->lock,FALSE);return s;
}
#endif
