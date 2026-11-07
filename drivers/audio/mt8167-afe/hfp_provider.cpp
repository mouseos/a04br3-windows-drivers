/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Audio-owned actual MRG/DL1+DAI DMA provider for Nash provider v2.
 * BT PnP/endpoint implementations and headers remain in Nash's source tree.
 */
#include "hfp_provider.h"
#include "hfp_memif.h"
#include <hfp_pnp.h>
#define TAG 'Hf4M'
struct HfpBuffer {
 PVOID cpu;PMDL mdl;PHYSICAL_ADDRESS logical;ULONG bytes,generation;
 EX_RUNDOWN_REF positions;BOOLEAN closed,exposed;
};
struct Mt8167HfpOwner {
 Mt8167HfpResources r;A04_SHARED_HFP_INTERFACE parent;A04_HFP_CONTROLLER controller;
 struct a04_bt_hfp_provider provider;mt6630_hfp_pnp *pnp;
 KMUTEX mutex;PKINTERRUPT interrupt;KDPC dpc;PIO_WORKITEM faultWork,finalWork;
 volatile LONG refs,faultQueued,failure,closed;ULONG generation,comboGeneration,bufferGeneration;
 mt8167_hfp_memif memif;HfpBuffer client[2],fallback[2];
 BOOLEAN started,clockHeld,comboHeld;
};
static void Lock(Mt8167HfpOwner *o){KeWaitForSingleObject(&o->mutex,Executive,KernelMode,FALSE,NULL);}
static void Unlock(Mt8167HfpOwner *o){KeReleaseMutex(&o->mutex,FALSE);}
/* Only local pinned mappings at device DIRQL. Controller Live is specified
 * <=DISPATCH and must never be called from our ISR/KeSynchronizeExecution. */
static BOOLEAN RegistersHeld(Mt8167HfpOwner *o)
{return !o->closed&&o->clockHeld&&o->comboHeld&&o->r.afe;}
static BOOLEAN HardwareLive(Mt8167HfpOwner *o)
{return !o->closed&&o->clockHeld&&o->comboHeld&&o->parent.Live(o->parent.Header.Context,o->generation)&&o->controller.Live(o->controller.context,o->comboGeneration);}
static BOOLEAN Live(Mt8167HfpOwner *o){return !o->failure&&HardwareLive(o);}
static int Read(void *c,unsigned off,uint32_t *v)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(!v||(off&3)||off>=0x1000||!RegistersHeld(o))return -1;*v=READ_REGISTER_ULONG((volatile ULONG *)((PUCHAR)o->r.afe+off));return 0;}
static int Write(void *c,unsigned off,uint32_t v)
{
 Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(!RegistersHeld(o))return -1;
 switch(off){case 0x10:case 0x14:case 0x1c:case 0x24:case 0x2c:case 0x3c:case 0x40:case 0x48:case 0x90:case 0x98:case 0x3a0:case 0x3a8:case 0x3ac:case 0x3b0:case 0x3b4:case 0x3d8:break;default:return -1;}
 WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)o->r.afe+off),v);return 0;
}
static void Reference(PVOID c){InterlockedIncrement(&((Mt8167HfpOwner *)c)->refs);}
/* Successful Stop drops the creator reference; retained KS/session references
 * keep the owner/backend/mapping alive until their final Release. Last-reference
 * cleanup runs only at PASSIVE, after proven DMA/IRQ quiescence. */
static VOID Finalize(PDEVICE_OBJECT,PVOID c)
{
 Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;
 if(o->pnp||o->clockHeld||o->comboHeld||o->interrupt)return;
 for(ULONG d=0;d<2;d++)if(o->client[d].mdl||o->fallback[d].mdl)return;
 if(o->controller.context){if(!o->controller.Dereference)return;
  o->controller.Dereference(o->controller.context);}
 if(o->parent.Header.InterfaceDereference)o->parent.Header.InterfaceDereference(o->parent.Header.Context);
 if(o->faultWork)IoFreeWorkItem(o->faultWork);if(o->finalWork)IoFreeWorkItem(o->finalWork);
 o->r.release(o->r.owner);ObDereferenceObject(o->r.fdo);ExFreePoolWithTag(o,TAG);
}
static void Dereference(PVOID c)
{
 Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(InterlockedDecrement(&o->refs))return;
 if(KeGetCurrentIrql()==PASSIVE_LEVEL)Finalize(NULL,o);
 else if(o->finalWork)IoQueueWorkItem(o->finalWork,Finalize,DelayedWorkQueue,o);
 /* Unknown owner state remains pinned; never retry finalization in a loop. */
}
static NTSTATUS Match(PVOID c,PDEVICE_OBJECT target)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(o->closed||o->failure)return STATUS_DEVICE_NOT_READY;return o->controller.MatchHfpTarget(o->controller.context,target);}
static NTSTATUS Validate(PVOID c,UCHAR codec,ULONG rate,ULONG channels,ULONG bits)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(codec||rate!=8000||channels!=1||bits!=16)return STATUS_NOT_SUPPORTED;
 return o->controller.ValidatePcm(o->controller.context,codec,rate,channels,bits);}
static NTSTATUS AllocateBuffer(Mt8167HfpOwner *o,ULONG bytes,HfpBuffer *b)
{
 if(b->mdl||!bytes||bytes>65536||o->bufferGeneration==MAXULONG)return STATUS_INVALID_PARAMETER;
 bytes=(bytes+PAGE_SIZE-1)&~(PAGE_SIZE-1);RtlZeroMemory(b,sizeof(*b));
 b->cpu=o->r.adapter->DmaOperations->AllocateCommonBuffer(o->r.adapter,bytes,&b->logical,FALSE);
 if(!b->cpu)return STATUS_INSUFFICIENT_RESOURCES;b->bytes=bytes;
 if(((ULONG_PTR)b->cpu&(PAGE_SIZE-1))||b->logical.QuadPart<=0||(b->logical.QuadPart&(PAGE_SIZE-1))||
    (ULONGLONG)b->logical.QuadPart>0x100000000ULL-bytes)goto invalid;
 RtlZeroMemory(b->cpu,bytes);b->mdl=IoAllocateMdl(b->cpu,bytes,FALSE,FALSE,NULL);
 if(!b->mdl)goto invalid;MmBuildMdlForNonPagedPool(b->mdl);
 if(MmGetMdlByteOffset(b->mdl)||MmGetMdlByteCount(b->mdl)!=bytes)goto invalid;
 KeMemoryBarrier();b->generation=++o->bufferGeneration;ExInitializeRundownProtection(&b->positions);return STATUS_SUCCESS;
 invalid:if(b->mdl)IoFreeMdl(b->mdl);o->r.adapter->DmaOperations->FreeCommonBuffer(o->r.adapter,bytes,b->logical,b->cpu,FALSE);RtlZeroMemory(b,sizeof(*b));return STATUS_DEVICE_CONFIGURATION_ERROR;
}
static NTSTATUS DrainBuffer(Mt8167HfpOwner *o,ULONG d,HfpBuffer *b)
{
 if(!b->mdl)return STATUS_SUCCESS;
 if(!b->closed){ExWaitForRundownProtectionRelease(&b->positions);b->closed=TRUE;}
 if(b->exposed){
  if(!o->parent.VerifyDmaQuiescent)return STATUS_NOT_SUPPORTED;
  NTSTATUS s=o->parent.VerifyDmaQuiescent(o->parent.Header.Context,o->generation,d,b->mdl);
  if(!NT_SUCCESS(s))return s;b->exposed=FALSE;
 }
 IoFreeMdl(b->mdl);o->r.adapter->DmaOperations->FreeCommonBuffer(o->r.adapter,b->bytes,b->logical,b->cpu,FALSE);
 RtlZeroMemory(b,sizeof(*b));return STATUS_SUCCESS;
}
static NTSTATUS Allocate(PVOID c,ULONG d,ULONG bytes,a04_bt_hfp_buffer *out)
{
 Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(d>1||!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(out,sizeof(*out));Lock(o);NTSTATUS s=o->closed||o->failure?STATUS_DEVICE_NOT_READY:AllocateBuffer(o,bytes,&o->client[d]);
 if(NT_SUCCESS(s)){out->mdl=o->client[d].mdl;out->bytes=o->client[d].bytes;out->generation=o->client[d].generation;out->cache=MmNonCached;}Unlock(o);return s;
}
static BOOLEAN BufferLive(PVOID c,ULONG d,ULONG g)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(d>1)return FALSE;HfpBuffer *b=&o->client[d];
 if(!ExAcquireRundownProtection(&b->positions))return FALSE;
 BOOLEAN live=!o->closed&&!o->failure&&b->mdl&&b->generation==g&&!b->closed;
 ExReleaseRundownProtection(&b->positions);return live;}
static NTSTATUS Free(PVOID c,ULONG d,const a04_bt_hfp_buffer *b)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(d>1||!b||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 Lock(o);NTSTATUS s=STATUS_DEVICE_BUSY;if(!(o->memif.active&(1u<<d))&&b->mdl==o->client[d].mdl&&b->generation==o->client[d].generation)s=DrainBuffer(o,d,&o->client[d]);Unlock(o);return s;}
struct Sync {Mt8167HfpOwner *o;ULONG d,g;BOOLEAN active;uint32_t offset;int result;};
static BOOLEAN PositionSync(PVOID p){Sync *s=(Sync *)p;s->result=mt8167_hfp_memif_position(&s->o->memif,s->d,s->g,&s->offset);return TRUE;}
static NTSTATUS Position(PVOID c,ULONG d,ULONG generation,ULONGLONG *play,ULONGLONG *write)
{
 Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(d>1||!play||!write)return STATUS_INVALID_PARAMETER;
 HfpBuffer *b=&o->client[d];if(!ExAcquireRundownProtection(&b->positions))return STATUS_DEVICE_NOT_READY;
 NTSTATUS status=STATUS_DEVICE_NOT_READY;
 if(Live(o)&&o->interrupt&&b->mdl&&generation==b->generation){Sync s={o,d,generation,FALSE,0,-1};KeSynchronizeExecution(o->interrupt,PositionSync,&s);if(!s.result){*play=s.offset;*write=s.offset;status=STATUS_SUCCESS;}}
 ExReleaseRundownProtection(&b->positions);return status;
}
static BOOLEAN PinSync(PVOID p)
{Sync *s=(Sync *)p;HfpBuffer *b=&s->o->client[s->d];mt8167_hfp_ring ring={(uint32_t)b->logical.QuadPart,b->bytes,b->generation};
 s->result=s->active?mt8167_hfp_memif_bind(&s->o->memif,s->d,&ring):mt8167_hfp_memif_inactive(&s->o->memif,s->d);return TRUE;}
static NTSTATUS Active(PVOID c,ULONG d,BOOLEAN active)
{
 Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(d>1||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 Lock(o);NTSTATUS status=STATUS_DEVICE_NOT_READY;
 if(Live(o)&&o->interrupt&&o->client[d].mdl){Sync s={o,d,0,active,0,-1};if(active)o->client[d].exposed=TRUE;
  KeSynchronizeExecution(o->interrupt,PinSync,&s);status=s.result?STATUS_IO_DEVICE_ERROR:STATUS_SUCCESS;
  if(NT_SUCCESS(status)&&!active){HfpBuffer *b=&o->client[d];if(!b->closed){ExWaitForRundownProtectionRelease(&b->positions);b->closed=TRUE;}
   status=o->parent.VerifyDmaQuiescent(o->parent.Header.Context,o->generation,d,b->mdl);
   if(NT_SUCCESS(status)){b->exposed=FALSE;ExReInitializeRundownProtection(&b->positions);b->closed=FALSE;}}
 }Unlock(o);return status;
}
static NTSTATUS Description(PVOID c,PDEVICE_DESCRIPTION out)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(!out||o->closed||!o->r.adapter)return STATUS_DEVICE_NOT_READY;
 RtlZeroMemory(out,sizeof(*out));out->Version=DEVICE_DESCRIPTION_VERSION;out->Master=TRUE;out->Dma32BitAddresses=TRUE;out->InterfaceType=Internal;out->MaximumLength=65536;return STATUS_SUCCESS;}
static NTSTATUS Latency(PVOID c,ULONG d,a04_bt_hfp_latency *out)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;UNREFERENCED_PARAMETER(o);UNREFERENCED_PARAMETER(d);UNREFERENCED_PARAMETER(out);
 /* AFE FIFO/transport latency needs a source-backed unit conversion. No zero
  * latency success is fabricated; this property is not a capability advert. */
 return STATUS_NOT_SUPPORTED;}
static BOOLEAN StopSync(PVOID c);
static VOID FaultWork(PDEVICE_OBJECT,PVOID c)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;Lock(o);
 /* Position and the ISR also touch memif state. PASSIVE work is not by itself
  * interrupt exclusion; use the same connected interrupt synchronization. */
 if(o->memif.prepared){if(o->interrupt)(void)KeSynchronizeExecution(o->interrupt,StopSync,o);
  else (void)mt8167_hfp_memif_stop_registers(&o->memif);}
 Unlock(o);Dereference(o);}
static void Fault(PVOID c,NTSTATUS status)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;InterlockedCompareExchange(&o->failure,(LONG)status,0);
 if(!InterlockedCompareExchange(&o->faultQueued,1,0)){Reference(o);IoQueueWorkItem(o->faultWork,FaultWork,DelayedWorkQueue,o);}}
static int AcquireLocked(PVOID c,unsigned rate,unsigned channels,unsigned bits,unsigned *generation)
{
 Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(!generation||rate!=8000||channels!=1||bits!=16||o->closed||o->failure)return -1;
 NTSTATUS s=o->controller.Acquire(o->controller.context,&o->comboGeneration);
 if(!NT_SUCCESS(s)){if(o->comboGeneration){o->comboHeld=TRUE;return -2;}return -1;}
 o->comboHeld=TRUE;if(!o->comboGeneration)return -2;
 s=o->parent.Acquire(o->parent.Header.Context,&o->generation);
 if(!NT_SUCCESS(s)){if(o->generation){o->clockHeld=TRUE;return -2;}if(!NT_SUCCESS(o->controller.Release(o->controller.context,o->comboGeneration)))return -2;o->comboHeld=FALSE;return -1;}
 o->clockHeld=TRUE;*generation=o->generation;return 0;
}
static int SbLive(PVOID c,unsigned g){Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;return g==o->generation&&HardwareLive(o)?1:0;}
static int SbRead(PVOID c,unsigned g,unsigned off,uint32_t *v){if(SbLive(c,g)!=1||(off!=0x1c&&off!=0x3c&&off!=0x24&&off!=0x2c))return -1;return Read(c,off,v);}
static int SbUpdate(PVOID c,unsigned g,unsigned off,uint32_t mask,uint32_t bits)
{uint32_t allowed=off==0x1c?0x120bu:off==0x3c?0xf10001u:off==0x24?0x3060u:off==0x2c?0x40u:0;
 uint32_t v;if(!mask||(mask&~allowed)||(bits&~mask)||SbLive(c,g)!=1||Read(c,off,&v)||Write(c,off,(v&~mask)|(bits&mask))||Read(c,off,&v)||(v&mask)!=(bits&mask))return -1;return 0;}
static int Pins(PVOID c,unsigned g,int enable)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;return NT_SUCCESS(o->parent.Pins(o->parent.Header.Context,g,enable?TRUE:FALSE))?0:-1;}
static int Delay(PVOID c,unsigned g,unsigned us){if(SbLive(c,g)!=1||us!=100||KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;KeStallExecutionProcessor(us);return 0;}
static VOID FaultDpc(PKDPC,PVOID c,PVOID,PVOID){Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;Fault(o,STATUS_IO_DEVICE_ERROR);}
static BOOLEAN Isr(PKINTERRUPT,PVOID c)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;uint32_t serviced=0;if(o->failure||!RegistersHeld(o)||!o->memif.prepared)return FALSE;
 if(mt8167_hfp_memif_irq(&o->memif,&serviced)){(void)mt8167_hfp_memif_stop_registers(&o->memif);KeInsertQueueDpc(&o->dpc,NULL,NULL);return FALSE;}return serviced?TRUE:FALSE;}
static int PrepareLocked(PVOID c,unsigned g,unsigned rate,unsigned channels,unsigned bits)
{
 Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(SbLive(c,g)!=1||rate!=8000||channels!=1||bits!=16)return -1;
 for(ULONG d=0;d<2;d++)if(!NT_SUCCESS(AllocateBuffer(o,PAGE_SIZE,&o->fallback[d])))return -2;
 mt8167_hfp_ring fallback[2];for(ULONG d=0;d<2;d++){fallback[d].base=(uint32_t)o->fallback[d].logical.QuadPart;fallback[d].bytes=o->fallback[d].bytes;fallback[d].generation=o->fallback[d].generation;}
 mt8167_hfp_io io={o,Read,Write};if(mt8167_hfp_memif_prepare(&o->memif,&io,fallback))return -2;
 NTSTATUS s=IoConnectInterrupt(&o->interrupt,Isr,o,NULL,o->r.vector,o->r.level,o->r.level,LevelSensitive,FALSE,o->r.affinity,FALSE);
 return NT_SUCCESS(s)?0:-2;
}
static BOOLEAN StartSync(PVOID c){Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;return mt8167_hfp_memif_start(&o->memif)==0;}
static BOOLEAN StopSync(PVOID c){Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;return mt8167_hfp_memif_stop_registers(&o->memif)==0;}
static int StartLocked(PVOID c,unsigned g)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(SbLive(c,g)!=1||!o->interrupt)return -1;
 for(ULONG d=0;d<2;d++)o->fallback[d].exposed=TRUE;
 return KeSynchronizeExecution(o->interrupt,StartSync,o)?0:-2;}
static int StopLocked(PVOID c,unsigned g)
{
 Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(SbLive(c,g)!=1)return -1;
 /* Block NEW position callbacks and wait for ones holding PKINTERRUPT before
  * IoDisconnectInterrupt releases that object. Register disable alone does
  * not prevent a concurrent GetPosition from dereferencing it. */
 for(ULONG d=0;d<2;d++){HfpBuffer *b=&o->client[d];if(b->mdl&&!b->closed){ExWaitForRundownProtectionRelease(&b->positions);b->closed=TRUE;}}
 if(o->interrupt){if(!KeSynchronizeExecution(o->interrupt,StopSync,o))return -2;IoDisconnectInterrupt(o->interrupt);o->interrupt=NULL;KeRemoveQueueDpc(&o->dpc);KeFlushQueuedDpcs();}
 for(ULONG d=0;d<2;d++){
  HfpBuffer *b=&o->client[d];if(b->exposed&&!NT_SUCCESS(o->parent.VerifyDmaQuiescent(o->parent.Header.Context,g,d,b->mdl)))return -2;
  b->exposed=FALSE;if(!NT_SUCCESS(DrainBuffer(o,d,&o->fallback[d])))return -2;
  /* A PAUSE/last RUN may retain its MDL for a later RUN. Reopen its position
   * gate only after drain proof and interrupt disconnection, never as proof. */
  if(b->mdl&&b->closed){ExReInitializeRundownProtection(&b->positions);b->closed=FALSE;}
 }
 o->memif.exposed=0;
 /* Allocation/IRQ-connect may fail before prepare wrote any registers. */
 if(!o->memif.prepared)return 0;
 return mt8167_hfp_memif_restore(&o->memif)?-2:0;
}
static int ReleaseLocked(PVOID c,unsigned g)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(SbLive(c,g)!=1||o->memif.prepared||o->interrupt)return -1;
 NTSTATUS s=o->parent.Release(o->parent.Header.Context,g);if(!NT_SUCCESS(s))return -2;o->clockHeld=FALSE;
 s=o->controller.Release(o->controller.context,o->comboGeneration);if(!NT_SUCCESS(s))return -2;o->comboHeld=FALSE;return 0;}
/* The BT session serializes its own stream calls, but does not exclude Audio
 * fault/power work. All state-changing sideband calls share the Audio gate. */
static int Acquire(PVOID c,unsigned r,unsigned n,unsigned bits,unsigned *g)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 Lock(o);int result=o->closed||o->failure?-1:AcquireLocked(c,r,n,bits,g);Unlock(o);return result;}
static int Prepare(PVOID c,unsigned g,unsigned r,unsigned n,unsigned bits)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 Lock(o);int result=o->closed||o->failure?-1:PrepareLocked(c,g,r,n,bits);Unlock(o);return result;}
static int Start(PVOID c,unsigned g)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 Lock(o);int result=o->closed||o->failure?-1:StartLocked(c,g);Unlock(o);return result;}
static int Stop(PVOID c,unsigned g)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 Lock(o);int result=StopLocked(c,g);Unlock(o);return result;}
static int Release(PVOID c,unsigned g)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 Lock(o);int result=ReleaseLocked(c,g);Unlock(o);return result;}

struct Retire {Mt8167HfpOwner *o;PIO_WORKITEM work;PWORKER_THREAD_ROUTINE cleanup;PVOID context;};
static VOID RetireWork(PDEVICE_OBJECT,PVOID c)
{Retire *r=(Retire *)c;Mt8167HfpOwner *o=r->o;
 if(!o->clockHeld&&!o->comboHeld&&!o->interrupt&&!o->failure)r->cleanup(r->context);
 /* Otherwise retain callback/context/code for actual owner rundown. */
 else return;
 IoFreeWorkItem(r->work);ExFreePoolWithTag(r,TAG);Dereference(o);}
static NTSTATUS RetireContext(PVOID c,PWORKER_THREAD_ROUTINE cleanup,PVOID context)
{Mt8167HfpOwner *o=(Mt8167HfpOwner *)c;if(!cleanup||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 Retire *r=(Retire *)ExAllocatePoolWithTag(NonPagedPool,sizeof(*r),TAG);if(!r)return STATUS_INSUFFICIENT_RESOURCES;
 r->work=IoAllocateWorkItem(o->r.fdo);if(!r->work){ExFreePoolWithTag(r,TAG);return STATUS_INSUFFICIENT_RESOURCES;}
 Reference(o);r->o=o;r->cleanup=cleanup;r->context=context;IoQueueWorkItem(r->work,RetireWork,DelayedWorkQueue,r);return STATUS_SUCCESS;}
static NTSTATUS QueryComplete(PDEVICE_OBJECT,PIRP,PVOID c)
{KeSetEvent((PKEVENT)c,IO_NO_INCREMENT,FALSE);return STATUS_MORE_PROCESSING_REQUIRED;}
static NTSTATUS Query(PDEVICE_OBJECT pdo,A04_SHARED_HFP_INTERFACE *out)
{
 KEVENT event;PIRP irp;KeInitializeEvent(&event,NotificationEvent,FALSE);
 irp=IoAllocateIrp(pdo->StackSize,FALSE);if(!irp)return STATUS_INSUFFICIENT_RESOURCES;
 PIO_STACK_LOCATION s=IoGetNextIrpStackLocation(irp);s->MajorFunction=IRP_MJ_PNP;s->MinorFunction=IRP_MN_QUERY_INTERFACE;
 s->Parameters.QueryInterface.InterfaceType=&A04SharedHfpInterfaceGuid;s->Parameters.QueryInterface.Size=sizeof(*out);
 s->Parameters.QueryInterface.Version=A04_SHARED_HFP_VERSION;s->Parameters.QueryInterface.Interface=(PINTERFACE)out;
 irp->IoStatus.Status=STATUS_NOT_SUPPORTED;
 IoSetCompletionRoutine(irp,QueryComplete,&event,TRUE,TRUE,TRUE);
 NTSTATUS status=IoCallDriver(pdo,irp);if(status==STATUS_PENDING)KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);
 status=irp->IoStatus.Status;IoFreeIrp(irp);return status;
}
NTSTATUS Mt8167HfpStart(const Mt8167HfpResources *r,Mt8167HfpOwner **out)
{
 if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
 if(!r||!r->fdo||!r->pdo||!r->resources||!r->afe||!r->adapter||!r->reference||!r->release||!r->vector||!r->affinity||!r->adapter->DmaOperations||
    !r->adapter->DmaOperations->AllocateCommonBuffer||!r->adapter->DmaOperations->FreeCommonBuffer||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 Mt8167HfpOwner *o=(Mt8167HfpOwner *)ExAllocatePoolWithTag(NonPagedPool,sizeof(*o),TAG);if(!o)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(o,sizeof(*o));o->r=*r;o->refs=1;r->reference(r->owner);ObReferenceObject(r->fdo);KeInitializeMutex(&o->mutex,0);KeInitializeDpc(&o->dpc,FaultDpc,o);
 o->finalWork=IoAllocateWorkItem(r->fdo);
 NTSTATUS status=o->finalWork?Query(r->pdo,&o->parent):STATUS_INSUFFICIENT_RESOURCES;if(!NT_SUCCESS(status))goto failed;
 if(o->parent.Header.Size!=sizeof(o->parent)||o->parent.Header.Version!=A04_SHARED_HFP_VERSION||
    !o->parent.Header.InterfaceReference||!o->parent.Header.InterfaceDereference||
    !o->parent.Acquire||!o->parent.Live||!o->parent.Pins||!o->parent.Release||
    !o->parent.GetController||!o->parent.VerifyDmaQuiescent){status=STATUS_NOT_SUPPORTED;goto failed;}
 status=o->parent.GetController(o->parent.Header.Context,&o->controller);if(!NT_SUCCESS(status))goto failed;
 if(o->controller.size!=sizeof(o->controller)||o->controller.version!=1||
    !o->controller.context||!o->controller.controller||!o->controller.Reference||!o->controller.Dereference||
    !o->controller.Acquire||!o->controller.Live||!o->controller.Release||
    !o->controller.MatchHfpTarget||!o->controller.ValidatePcm||!o->controller.CodecLatency){status=STATUS_NOT_SUPPORTED;goto failed;}
 o->faultWork=IoAllocateWorkItem(r->fdo);if(!o->faultWork){status=STATUS_INSUFFICIENT_RESOURCES;goto failed;}
 o->provider.size=sizeof(o->provider);o->provider.version=2;o->provider.context=o;
 o->provider.reference=Reference;o->provider.dereference=Dereference;o->provider.match_hfp_target=Match;o->provider.validate_pcm=Validate;
 o->provider.allocate=Allocate;o->provider.buffer_live=BufferLive;o->provider.free_drained=Free;o->provider.set_pin_active=Active;
 o->provider.position=Position;o->provider.device_description=Description;o->provider.latency=Latency;o->provider.fault=Fault;o->provider.retire=RetireContext;
 o->provider.sideband={o,Acquire,SbLive,SbRead,SbUpdate,Pins,Prepare,Start,Stop,Delay,Release};
 status=Mt6630HfpPnpStart(r->fdo,r->resources,&o->provider,&o->pnp);
 if(!NT_SUCCESS(status)&&o->pnp){*out=o;return status;}
 if(!NT_SUCCESS(status))goto failed;*out=o;return STATUS_SUCCESS;
 failed:Dereference(o);return status;
}
BOOLEAN Mt8167HfpCanSuspend(Mt8167HfpOwner *o)
{if(!o||KeGetCurrentIrql()!=PASSIVE_LEVEL)return FALSE;Lock(o);
 BOOLEAN quiescent=!o->clockHeld&&!o->comboHeld&&!o->interrupt&&!o->failure;
 Unlock(o);return quiescent;}
NTSTATUS Mt8167HfpStop(Mt8167HfpOwner *o)
{
 if(!o||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 NTSTATUS status=o->pnp?Mt6630HfpPnpStop(o->pnp):STATUS_SUCCESS;if(!NT_SUCCESS(status))return status;o->pnp=NULL;
 InterlockedExchange(&o->closed,1);if(o->clockHeld||o->comboHeld||o->interrupt)return STATUS_DEVICE_BUSY;
 for(ULONG d=0;d<2;d++)if(o->client[d].exposed||o->fallback[d].mdl)return STATUS_DEVICE_BUSY;
 /* KS handles may retain unexposed/drained client MDLs after unregister. The
  * session's real provider Reference pins this owner until free_drained and
  * final Release. Do not require handle disappearance to complete PnP Stop. */
 Dereference(o);return STATUS_SUCCESS;
}
