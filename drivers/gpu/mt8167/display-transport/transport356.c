/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * DIS0 PnP resource/DMA/real frame-IRQ provider. No DxgkInitialize, second GPU
 * owner, framebuffer CPU blitter, root registration or user control device. */
#include "interface356.h"
#include "../start_checkpoint357.h"
#include <acpiioct.h>
#include "../vendor_m1_11/windows_shared_scanout.h"
#include "barrier356.h"
#include "post_profile356.h"
#define TAG '6DsG'
#define FRAME_BYTES 4096000u
#define FRAME_PAGES 1000u
#define OVL_INTEN 0x004u
#define OVL_INTSTA 0x008u
#define OVL_EN 0x00cu
#define OVL_RST 0x014u
#define OVL_ROI 0x020u
#define OVL_SRC 0x02cu
#define OVL_CON0 0x030u
#define OVL_SIZE0 0x038u
#define OVL_PITCH0 0x044u
#define OVL_RDMA0 0x0c0u
#define OVL_FLOW 0x240u
#define OVL_ADDR0 0xf40u
struct display356;
static NTSTATUS platform_proof(struct display356 *);
static void put_sg(struct display356 *,PSCATTER_GATHER_LIST);
struct map356 {
 LIST_ENTRY entry;struct display356 *owner;struct geo_frontend_mdl_lease lease;
 PSCATTER_GATHER_LIST sg;KEVENT event;uint32_t dma;BOOLEAN exposed,quarantined;
};
struct display356 {
 PDEVICE_OBJECT self,pdo,lower;UNICODE_STRING link;volatile ULONG *ovl,*rdma;
 PKINTERRUPT interrupt,ovl_interrupt;PDMA_ADAPTER dma;ULONG map_registers;
 CM_RESOURCE_LIST brightness;
 KMUTEX mutex;LIST_ENTRY maps;LONG references;BOOLEAN started,closing,quarantined;
 ULONG saved[27];
 ULONG old_address,old_source,old_con,old_rdma,old_pitch;struct map356 *active;
 ULONGLONG frame_serial,ovl_serial;volatile LONG frame_errors,in_blank,seen_vsync;NTSTATUS last_status;
 a04_display_frame_notify notify;void *notify_context;
 struct geo_start357_record start357;BOOLEAN tracing357;volatile LONG d0_observed357;
};
static NTSTATUS fail357(struct display356 *d,ULONG id,NTSTATUS status){if(d->tracing357)Geo357Fail(&d->start357,id,status);return status;}
static ULONG sample357(struct display356 *d,ULONG offset,ULONG word){ULONG value=READ_REGISTER_ULONG(d->ovl+offset/4u);if(d->tracing357){d->start357.words[word]=value;d->start357.valid_flags|=(1u<<(word-24));}return value;}
static ULONG sample_addr357(struct display356 *d){ULONG value=READ_REGISTER_ULONG(d->ovl+OVL_ADDR0/4u);if(d->tracing357){d->start357.words[29]=(value?1u:0u)|((ULONGLONG)value>=0xbe9c0000ULL&&(ULONGLONG)value+FRAME_BYTES<=0xbf9c0000ULL?2u:0u);d->start357.valid_flags|=1u<<5;}return value;}
static ULONG sample_rdma357(struct display356 *d){ULONG value=READ_REGISTER_ULONG(d->rdma);if(d->tracing357){d->start357.words[31]=value;d->start357.valid_flags|=1u<<7;}return value;}
static ULONG rd(struct display356 *d,ULONG o){return READ_REGISTER_ULONG(d->ovl+o/4u);}
static void wr(struct display356 *d,ULONG o,ULONG v){WRITE_REGISTER_ULONG(d->ovl+o/4u,v);}
static const ULONG owned_offsets356[27]={0x020,0x024,0x028,
 0x030,0x038,0x03c,0x044,0x0c0,0xf40,
 0x050,0x058,0x05c,0x064,0x0e0,0xf60,
 0x070,0x078,0x07c,0x084,0x100,0xf80,
 0x090,0x098,0x09c,0x0a4,0x120,0xfa0};
static void restore_configuration356(struct display356 *d){for(ULONG i=0;i<27;i++)wr(d,owned_offsets356[i],d->saved[i]);}
static void reference(void *p){struct display356 *d=p;ObReferenceObject(d->self);InterlockedIncrement(&d->references);}
static void dereference(void *p){struct display356 *d=p;InterlockedDecrement(&d->references);ObDereferenceObject(d->self);}
static BOOLEAN irq(PKINTERRUPT interrupt,void *context){
 struct display356 *d=context;ULONG raw,status;
 if(!d->started||d->closing||!d->ovl||!d->rdma)return FALSE;
 if(interrupt==d->ovl_interrupt){
  raw=rd(d,OVL_INTSTA);status=raw&rd(d,OVL_INTEN);if(!status)return FALSE;
  InterlockedOr(&d->frame_errors,(LONG)(raw&0x1fe4u));wr(d,OVL_INTSTA,~status);
  if(status&2u)d->ovl_serial++;return TRUE;
 }
 /* CT3 primary_display.c maps video-mode IF_VSYNC to RDMA0_DONE bit2.
  * OVL frame completion alone is never synthesized into CRTC vsync. */
 raw=READ_REGISTER_ULONG(d->rdma+1);status=raw&READ_REGISTER_ULONG(d->rdma);if(!status)return FALSE;
 InterlockedOr(&d->frame_errors,(LONG)((raw&0x18u)<<16));WRITE_REGISTER_ULONG(d->rdma+1,~status);
 if(status&2u)InterlockedExchange(&d->in_blank,0);
 if(status&4u){InterlockedExchange(&d->in_blank,1);InterlockedExchange(&d->seen_vsync,1);d->frame_serial++;
  if(d->notify)d->notify(d->notify_context,d->frame_serial);}
 return TRUE;
}
struct frame_read {struct display356 *d;ULONGLONG serial;ULONG errors;BOOLEAN ovl;};
static BOOLEAN read_frame_locked(void *p){struct frame_read *q=p;q->serial=q->ovl?q->d->ovl_serial:q->d->frame_serial;q->errors=q->d->frame_errors;return TRUE;}
static ULONGLONG frame_serial(struct display356 *d){struct frame_read q={d,0,0,FALSE};return d->interrupt&&KeSynchronizeExecution(d->interrupt,read_frame_locked,&q)?q.serial:0;}
static ULONGLONG ovl_serial(struct display356 *d){struct frame_read q={d,0,0,TRUE};return d->ovl_interrupt&&KeSynchronizeExecution(d->ovl_interrupt,read_frame_locked,&q)?q.serial:0;}
static NTSTATUS idle(struct display356 *d){
 LARGE_INTEGER delay;ULONGLONG start=KeQueryInterruptTime();delay.QuadPart=-10000;
 for(ULONG i=0;i<100;i++){
  ULONG state=rd(d,OVL_FLOW);
  /* CT3 ovl_is_idle: FSM1/2; require RDMA0 idle in addition. */
  if(((state&0x3ffu)==1u||(state&0x3ffu)==2u)&&(state&(1u<<19)))return STATUS_SUCCESS;
  if(KeQueryInterruptTime()-start>=1000000u)break;
  (void)KeDelayExecutionThread(KernelMode,FALSE,&delay);
 }
 return STATUS_IO_TIMEOUT;
}
static NTSTATUS disable_drain(struct display356 *d){
 wr(d,OVL_INTEN,0);WRITE_REGISTER_ULONG(d->rdma,0);wr(d,OVL_EN,0);wr(d,OVL_RST,1);wr(d,OVL_RST,0);display_barrier356();
 NTSTATUS s=idle(d);if(!NT_SUCCESS(s))d->quarantined=TRUE;return s;
}
static NTSTATUS brightness_resources(void *p,PCM_RESOURCE_LIST *out){
 struct display356 *d=p;if(!d||!out||!d->started||d->closing||!d->references||d->brightness.Count!=1)return STATUS_DEVICE_NOT_READY;
 *out=&d->brightness;return STATUS_SUCCESS;
}
static NTSTATUS snapshot(void *p,struct a04_display_transport_snapshot *o){
 struct display356 *d=p;ULONG count=0;
 if(!d||!o||o->size!=sizeof(*o)||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 KeWaitForSingleObject(&d->mutex,Executive,KernelMode,FALSE,NULL);
 for(LIST_ENTRY *e=d->maps.Flink;e!=&d->maps;e=e->Flink)count++;
 RtlZeroMemory(o,sizeof(*o));o->size=sizeof(*o);o->version=1;o->ready=d->started&&!d->closing&&!d->quarantined;
 o->width=800;o->height=1280;o->pitch=3200;o->format=21;o->post_address=d->old_address;
 o->active_address=d->active?d->active->dma:d->old_address;o->frame_serial=frame_serial(d);o->last_status=d->last_status;o->retained_maps=count;
 o->restore_completed=!d->active&&!d->quarantined;KeReleaseMutex(&d->mutex,FALSE);return STATUS_SUCCESS;
}
static void mapped(PDEVICE_OBJECT pdo,PIRP irp,PSCATTER_GATHER_LIST sg,void *context){
 struct map356 *m=context;UNREFERENCED_PARAMETER(pdo);UNREFERENCED_PARAMETER(irp);
 m->sg=sg;KeSetEvent(&m->event,IO_NO_INCREMENT,FALSE);
}
static NTSTATUS map_backing(void *p,const struct geo_frontend_mdl_lease *lease,void **cookie){
 struct display356 *d=p;struct map356 *m;NTSTATUS s;KIRQL old_irql;
 if(!d||!lease||!cookie||KeGetCurrentIrql()!=PASSIVE_LEVEL||lease->size!=sizeof(*lease)||lease->version!=1||
    lease->origin!=GEO_MDL_VIDMM_CACHED_SHADOW||lease->active_bytes!=FRAME_BYTES||lease->page_bytes!=FRAME_BYTES||!lease->mdl||!lease->acquire||!lease->release||!lease->context)return STATUS_INVALID_PARAMETER;
 *cookie=NULL;
 if(MmGetMdlByteOffset(lease->mdl)||MmGetMdlByteCount(lease->mdl)!=FRAME_BYTES||!(lease->mdl->MdlFlags&MDL_PAGES_LOCKED)||(lease->mdl->MdlFlags&MDL_IO_SPACE))return STATUS_INVALID_PARAMETER;
 KeWaitForSingleObject(&d->mutex,Executive,KernelMode,FALSE,NULL);
 if(!d->started||d->closing||d->quarantined){s=STATUS_DEVICE_NOT_READY;goto done;}
 s=platform_proof(d);if(!NT_SUCCESS(s))goto done;
 m=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*m),TAG);if(!m){s=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 RtlZeroMemory(m,sizeof(*m));m->owner=d;m->lease=*lease;KeInitializeEvent(&m->event,NotificationEvent,FALSE);
 s=lease->acquire(lease->context,lease->mdl);if(!NT_SUCCESS(s)){ExFreePoolWithTag(m,TAG);goto done;}
 reference(d);InsertTailList(&d->maps,&m->entry);
 KeRaiseIrql(DISPATCH_LEVEL,&old_irql);
 s=d->dma->DmaOperations->GetScatterGatherList(d->dma,d->pdo,lease->mdl,MmGetMdlVirtualAddress(lease->mdl),FRAME_BYTES,mapped,m,TRUE);
 KeLowerIrql(old_irql);
 if(!NT_SUCCESS(s))goto failed;
 /* The HAL owns this callback context until signal. Returning a timeout
  * while releasing it would be UAF. No GPU/OVL exposure occurs in this wait. */
 do{s=KeWaitForSingleObject(&m->event,Executive,KernelMode,FALSE,NULL);}while(s!=STATUS_SUCCESS);
 if(!m->sg||!m->sg->NumberOfElements){s=STATUS_DEVICE_CONFIGURATION_ERROR;goto failed;}
 s=a04_shared_scanout_match_vidmm(lease->mdl,m->sg,&m->dma);if(!NT_SUCCESS(s))goto failed;
 *cookie=m;s=STATUS_SUCCESS;goto done;
failed:
 if(m->sg)put_sg(d,m->sg);
 RemoveEntryList(&m->entry);m->lease.release(m->lease.context,m->lease.mdl);ExFreePoolWithTag(m,TAG);dereference(d);
done:KeReleaseMutex(&d->mutex,FALSE);return s;
}
static void put_sg(struct display356 *d,PSCATTER_GATHER_LIST sg){
 KIRQL old;KeRaiseIrql(DISPATCH_LEVEL,&old);d->dma->DmaOperations->PutScatterGatherList(d->dma,sg,TRUE);KeLowerIrql(old);
}
static NTSTATUS unmap_backing(void *p,void *cookie){
 struct display356 *d=p;struct map356 *m=cookie;
 if(!d||!m||m->owner!=d||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 KeWaitForSingleObject(&d->mutex,Executive,KernelMode,FALSE,NULL);
 if(m->exposed||m->quarantined||d->active==m){KeReleaseMutex(&d->mutex,FALSE);return STATUS_DEVICE_BUSY;}
 put_sg(d,m->sg);RemoveEntryList(&m->entry);
 m->lease.release(m->lease.context,m->lease.mdl);ExFreePoolWithTag(m,TAG);KeReleaseMutex(&d->mutex,FALSE);dereference(d);return STATUS_SUCCESS;
}
static NTSTATUS select_backing(void *p,void *cookie){
 struct display356 *d=p;struct map356 *m=cookie,*old;NTSTATUS s;LARGE_INTEGER timeout;ULONGLONG before,before_ovl,start;
 if(!d||!m||m->owner!=d||!m->sg||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 KeWaitForSingleObject(&d->mutex,Executive,KernelMode,FALSE,NULL);
 if(!d->started||d->closing||d->quarantined){s=STATUS_DEVICE_NOT_READY;goto done;}
 s=platform_proof(d);if(!NT_SUCCESS(s))goto done;
 s=disable_drain(d);if(!NT_SUCCESS(s))goto done;
 old=d->active;if(old)old->exposed=FALSE;
 /* DMA has actually stopped, so the old scanout has retired. Retain the
  * new backing BEFORE the first register write; a timeout quarantines it. */
 d->active=m;m->exposed=TRUE;before=frame_serial(d);before_ovl=ovl_serial(d);
 restore_configuration356(d);
 wr(d,OVL_ROI,(1280u<<16)|800u);wr(d,OVL_CON0,0x20ffu);wr(d,OVL_SIZE0,(1280u<<16)|800u);
 wr(d,OVL_PITCH0,(d->old_pitch&0xffff0000u)|3200u);wr(d,OVL_ADDR0,m->dma);wr(d,OVL_RDMA0,d->old_rdma);wr(d,OVL_SRC,1u);
 wr(d,OVL_INTSTA,0);WRITE_REGISTER_ULONG(d->rdma+1,0);wr(d,OVL_INTEN,2u);WRITE_REGISTER_ULONG(d->rdma,6u);display_barrier356();wr(d,OVL_EN,1u);
 start=KeQueryInterruptTime();s=STATUS_IO_TIMEOUT;
 for(ULONG i=0;i<100;i++){
  if(frame_serial(d)>before&&ovl_serial(d)>before_ovl){s=STATUS_SUCCESS;break;}
  if(KeQueryInterruptTime()-start>=1000000u)break;
  /* No event signalling at DIRQL. Success still requires actual IRQ serial progress. */
  timeout.QuadPart=-10000;(void)KeDelayExecutionThread(KernelMode,FALSE,&timeout);
 }
 if(s!=STATUS_SUCCESS||d->frame_errors||rd(d,OVL_ADDR0)!=m->dma){m->quarantined=TRUE;d->quarantined=TRUE;s=STATUS_IO_TIMEOUT;}

 done:d->last_status=s;KeReleaseMutex(&d->mutex,FALSE);return s;
}
static NTSTATUS restore(void *p){
 struct display356 *d=p;NTSTATUS s;
 if(!d||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 KeWaitForSingleObject(&d->mutex,Executive,KernelMode,FALSE,NULL);
 if(!d->started){s=STATUS_DEVICE_NOT_READY;goto done;}
 s=disable_drain(d);if(!NT_SUCCESS(s))goto done;
 restore_configuration356(d);
 wr(d,OVL_CON0,d->old_con);wr(d,OVL_PITCH0,d->old_pitch);wr(d,OVL_ADDR0,d->old_address);wr(d,OVL_RDMA0,d->old_rdma);wr(d,OVL_SRC,d->old_source);display_barrier356();
 /* Do not re-enable DMA or drop the exposed mapping on an unconfirmed
  * address/configuration write. Idle alone does not prove future no-read. */
 if(rd(d,OVL_ADDR0)!=d->old_address||rd(d,OVL_CON0)!=d->old_con||rd(d,OVL_PITCH0)!=d->old_pitch||rd(d,OVL_SRC)!=d->old_source){s=STATUS_IO_DEVICE_ERROR;d->quarantined=TRUE;if(d->active)d->active->quarantined=TRUE;goto done;}
 wr(d,OVL_EN,1);display_barrier356();
 if(!(rd(d,OVL_EN)&1u)){s=STATUS_IO_DEVICE_ERROR;d->quarantined=TRUE;if(d->active)d->active->quarantined=TRUE;goto done;}
 if(d->active){d->active->exposed=FALSE;d->active->quarantined=FALSE;d->active=NULL;}
 d->quarantined=FALSE;
 /* Retirement proved above; restore is not a new GPU/frame completion. */
 done:d->last_status=s;KeReleaseMutex(&d->mutex,FALSE);return s;
}
static void emergency_restore(void *p){
 struct display356 *d=p;if(!d||!d->started||!d->ovl||!d->rdma)return;
 /* Any-IRQL panic fallback. No wait, allocation, unmap or retirement claim. */
 d->quarantined=TRUE;wr(d,OVL_INTEN,0);WRITE_REGISTER_ULONG(d->rdma,0);wr(d,OVL_EN,0);
 restore_configuration356(d);
 wr(d,OVL_CON0,d->old_con);wr(d,OVL_PITCH0,d->old_pitch);wr(d,OVL_ADDR0,d->old_address);wr(d,OVL_RDMA0,d->old_rdma);wr(d,OVL_SRC,d->old_source);display_barrier356();wr(d,OVL_EN,1);
}
struct subscription {struct display356 *d;a04_display_frame_notify notify;void *context;};
static BOOLEAN set_subscription(void *p){struct subscription *q=p;q->d->notify=q->notify;q->d->notify_context=q->context;return TRUE;}
static NTSTATUS subscribe(void *p,a04_display_frame_notify notify,void *context){
 struct display356 *d=p;struct subscription q={d,notify,context};
 if(!d||!d->interrupt||!notify||!context||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 return KeSynchronizeExecution(d->interrupt,set_subscription,&q)?STATUS_SUCCESS:STATUS_IO_DEVICE_ERROR;
}
static NTSTATUS unsubscribe(void *p){struct display356 *d=p;struct subscription q={d,NULL,NULL};
 if(!d||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(!d->interrupt){d->notify=NULL;d->notify_context=NULL;return STATUS_SUCCESS;}
 return KeSynchronizeExecution(d->interrupt,set_subscription,&q)?STATUS_SUCCESS:STATUS_IO_DEVICE_ERROR;
}
static NTSTATUS raster(void *p,ULONG *line,BOOLEAN *blank){
 struct display356 *d=p;ULONG counter;
 if(!d||!line||!blank||!d->started||d->closing||!d->rdma||!InterlockedCompareExchange(&d->seen_vsync,0,0))return STATUS_DEVICE_NOT_READY;
 /* CT3 DISP_REG_RDMA_OUT_LINE_CNT, actual output raster counter. */
 counter=READ_REGISTER_ULONG(d->rdma+0xfcu/4u);
 *blank=InterlockedCompareExchange(&d->in_blank,0,0)?TRUE:FALSE;
 if(*blank){*line=0;return STATUS_SUCCESS;}
 if(counter>=1280u)return STATUS_DEVICE_NOT_READY;
 *line=counter;return STATUS_SUCCESS;
}
static NTSTATUS complete(PIRP irp,NTSTATUS status){irp->IoStatus.Status=status;IoCompleteRequest(irp,IO_NO_INCREMENT);return status;}
static NTSTATUS forward(PDEVICE_OBJECT device,PIRP irp){struct display356 *d=device->DeviceExtension;IoSkipCurrentIrpStackLocation(irp);return IoCallDriver(d->lower,irp);}
static NTSTATUS forward_done(PDEVICE_OBJECT device,PIRP irp,void *p){UNREFERENCED_PARAMETER(device);UNREFERENCED_PARAMETER(irp);KeSetEvent(p,IO_NO_INCREMENT,FALSE);return STATUS_MORE_PROCESSING_REQUIRED;}
static NTSTATUS synchronous(struct display356 *d,PIRP irp){KEVENT e;NTSTATUS s;KeInitializeEvent(&e,NotificationEvent,FALSE);IoCopyCurrentIrpStackLocationToNext(irp);IoSetCompletionRoutine(irp,forward_done,&e,TRUE,TRUE,TRUE);s=IoCallDriver(d->lower,irp);if(s==STATUS_PENDING){KeWaitForSingleObject(&e,Executive,KernelMode,FALSE,NULL);}return irp->IoStatus.Status;}
static NTSTATUS platform_proof(struct display356 *d){
 ACPI_EVAL_INPUT_BUFFER in;UCHAR buffer[256];ACPI_EVAL_OUTPUT_BUFFER *out=(void *)buffer;
 KEVENT event;IO_STATUS_BLOCK iosb;PIRP irp;NTSTATUS s;ULONG words[8];
 RtlZeroMemory(&in,sizeof(in));RtlZeroMemory(buffer,sizeof(buffer));in.Signature=ACPI_EVAL_INPUT_BUFFER_SIGNATURE;RtlCopyMemory(in.MethodName,"DMS0",4);
 KeInitializeEvent(&event,NotificationEvent,FALSE);
 irp=IoBuildDeviceIoControlRequest(IOCTL_ACPI_EVAL_METHOD,d->lower,&in,sizeof(in),buffer,sizeof(buffer),FALSE,&event,&iosb);
 if(!irp)return fail357(d,210,STATUS_INSUFFICIENT_RESOURCES);s=IoCallDriver(d->lower,irp);if(d->tracing357)d->start357.words[16]=(ULONG)s;
 if(s==STATUS_PENDING){KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);s=iosb.Status;}
 if(d->tracing357)d->start357.words[17]=(ULONG)s;
 if(!NT_SUCCESS(s))return fail357(d,211,s);
 if(d->tracing357){d->start357.words[18]=out->Signature;d->start357.words[19]=out->Length;d->start357.words[20]=out->Count;d->start357.words[21]=out->Argument[0].Type;d->start357.words[22]=out->Argument[0].DataLength;}
 if(out->Signature!=ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE||out->Length>sizeof(buffer)||out->Count!=1||out->Argument[0].Type!=ACPI_METHOD_ARGUMENT_BUFFER||out->Argument[0].DataLength!=sizeof(words))return fail357(d,212,STATUS_DEVICE_CONFIGURATION_ERROR);
 RtlCopyMemory(words,out->Argument[0].Data,sizeof(words));if(d->tracing357){for(ULONG i=0;i<8;i++)d->start357.words[8+i]=words[i];d->start357.words[23]=1;}
 /* DMS0: live AML-owned SMI_LARB0_MMU_EN (OVL port0), SYSIRQ160,
  * and explicit fixed display profile. No guessed MMIO in this driver. */
 if(d->tracing357){ULONG id=words[0]!=0x36354d44u?213u:words[1]!=1u?214u:(words[2]&1u)?215u:((words[3]&5u)!=5u)?216u:words[4]!=800u?217u:words[5]!=1280u?218u:words[6]!=3200u?219u:words[7]!=1u?220u:0u;if(id)Geo357Fail(&d->start357,id,STATUS_DEVICE_CONFIGURATION_ERROR);}
 return words[0]==0x36354d44u&&words[1]==1u&&!(words[2]&1u)&&((words[3]&5u)==5u)&&words[4]==800&&words[5]==1280&&words[6]==3200&&words[7]==1?STATUS_SUCCESS:STATUS_DEVICE_CONFIGURATION_ERROR;
}
/* Read-only boot-owned panel timing. No Raster/IRQ prerequisite and no MMIO
 * writes. The referenced interface prevents Stop/Remove during evaluation. */
static NTSTATUS panel_timing(void *context,struct a04_panel_timing *timing){
 struct display356 *d=context;ACPI_EVAL_INPUT_BUFFER in;
 /* DWORD storage guarantees ACPI output alignment. */
 ULONG storage[64];ACPI_EVAL_OUTPUT_BUFFER *out=(void *)storage;
 KEVENT event;IO_STATUS_BLOCK iosb;PIRP irp;NTSTATUS status;
 struct a04_panel_timing record;struct a04_panel_frequencies frequencies;
 ULONG prefix=FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER,Argument)+FIELD_OFFSET(ACPI_METHOD_ARGUMENT,Data);
 if(!d||!timing||timing->size!=sizeof(*timing)||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(timing,sizeof(*timing));
 if(!d->started||d->closing||!d->references)return STATUS_DEVICE_NOT_READY;
 RtlZeroMemory(&in,sizeof(in));RtlZeroMemory(storage,sizeof(storage));RtlZeroMemory(&iosb,sizeof(iosb));
 in.Signature=ACPI_EVAL_INPUT_BUFFER_SIGNATURE;RtlCopyMemory(in.MethodName,"DTM0",4);
 KeInitializeEvent(&event,NotificationEvent,FALSE);
 irp=IoBuildDeviceIoControlRequest(IOCTL_ACPI_EVAL_METHOD,d->lower,&in,sizeof(in),storage,sizeof(storage),FALSE,&event,&iosb);
 if(!irp)return STATUS_INSUFFICIENT_RESOURCES;
 status=IoCallDriver(d->lower,irp);
 if(status==STATUS_PENDING){KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);status=iosb.Status;}
 if(!NT_SUCCESS(status))return status;
 if(iosb.Information<prefix+sizeof(record)||iosb.Information>sizeof(storage)||
    out->Signature!=ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE||out->Length!=prefix+sizeof(record)||
    out->Length>iosb.Information||out->Count!=1||
    out->Argument[0].Type!=ACPI_METHOD_ARGUMENT_BUFFER||out->Argument[0].DataLength!=sizeof(record))return STATUS_DEVICE_CONFIGURATION_ERROR;
 RtlCopyMemory(&record,out->Argument[0].Data,sizeof(record));
 if(record.magic!=A04_PANEL_TIMING_MAGIC||record.version!=1||record.size!=sizeof(record)||
    record.reserved[0]||record.reserved[1]||record.reserved[2]||record.reserved[3]||record.reserved[4])return STATUS_DEVICE_CONFIGURATION_ERROR;
 if(record.flags==0)return STATUS_DEVICE_NOT_READY;
 if(!a04_panel_timing_frequencies(&record,&frequencies))return STATUS_DEVICE_CONFIGURATION_ERROR;
 *timing=record;return STATUS_SUCCESS;
}
static NTSTATUS start(struct display356 *d,PCM_RESOURCE_LIST resources,PCM_RESOURCE_LIST raw){
 PCM_PARTIAL_RESOURCE_DESCRIPTOR ovl=NULL,rdma=NULL,intr=NULL,ovl_intr=NULL,pwm=NULL,post=NULL;ULONG ovls=0,rdmas=0,irqs=0,pwms=0,posts=0;DEVICE_DESCRIPTION description;NTSTATUS s;
 if(d->tracing357){d->start357.words[2]=raw?raw->Count:0xffffffffu;d->start357.words[3]=resources?resources->Count:0xffffffffu;if(raw&&raw->Count==1)d->start357.words[4]=raw->List[0].PartialResourceList.Count;if(resources&&resources->Count==1)d->start357.words[5]=resources->List[0].PartialResourceList.Count;}
 if(!raw||raw->Count!=1||!resources||resources->Count!=1||raw->List[0].PartialResourceList.Count!=resources->List[0].PartialResourceList.Count||resources->List[0].PartialResourceList.Count>16||d->started||!IsListEmpty(&d->maps))return fail357(d,201,STATUS_INVALID_PARAMETER);
 s=platform_proof(d);if(!NT_SUCCESS(s))return s;
 for(ULONG j=0;j<resources->Count;j++){
  PCM_PARTIAL_RESOURCE_LIST list=&resources->List[j].PartialResourceList;
  for(ULONG i=0;i<list->Count;i++){
   PCM_PARTIAL_RESOURCE_DESCRIPTOR r=&list->PartialDescriptors[i];
   if(d->tracing357){ULONG *w=&d->start357.words[48+i*8];w[0]=r->Type|((ULONG)r->ShareDisposition<<8)|((ULONG)r->Flags<<16);w[1]=raw->List[j].PartialResourceList.PartialDescriptors[i].Type;if(r->Type==CmResourceTypeMemory){w[2]=r->u.Memory.Length;w[3]=r->u.Memory.Start.QuadPart==0x14007000?1u:r->u.Memory.Start.QuadPart==0x14009000?2u:r->u.Memory.Start.QuadPart==0x14012000?3u:r->u.Memory.Start.QuadPart==0x14015000?4u:r->u.Memory.Start.QuadPart==0xbe9c0000?5u:r->u.Memory.Start.QuadPart==0x1100f000?6u:0u;}if(r->Type==CmResourceTypeInterrupt){w[4]=raw->List[j].PartialResourceList.PartialDescriptors[i].u.Interrupt.Vector;w[5]=r->u.Interrupt.Level;w[6]=r->u.Interrupt.Vector;w[7]=(ULONG)r->u.Interrupt.Affinity;}}

   if(r->Type==CmResourceTypeMemory&&r->u.Memory.Start.QuadPart==0x14007000&&r->u.Memory.Length==4096&&r->ShareDisposition==CmResourceShareDeviceExclusive){ovl=r;ovls++;}
   if(r->Type==CmResourceTypeMemory&&r->u.Memory.Start.QuadPart==0x14009000&&r->u.Memory.Length==4096&&r->ShareDisposition==CmResourceShareDeviceExclusive){rdma=r;rdmas++;}
   if(r->Type==CmResourceTypeMemory&&r->u.Memory.Start.QuadPart==0x1100f000&&r->u.Memory.Length==4096&&r->ShareDisposition==CmResourceShareDeviceExclusive){pwm=r;pwms++;}
   if(r->Type==CmResourceTypeMemory&&r->u.Memory.Start.QuadPart==0xbe9c0000&&r->u.Memory.Length==0x1000000&&r->ShareDisposition==CmResourceShareDeviceExclusive){post=r;posts++;}
   if(r->Type==CmResourceTypeInterrupt){
    PCM_PARTIAL_RESOURCE_DESCRIPTOR r0=&raw->List[j].PartialResourceList.PartialDescriptors[i];
    if(r0->Type!=CmResourceTypeInterrupt)return fail357(d,230,STATUS_DEVICE_CONFIGURATION_ERROR);
    if(r0->u.Interrupt.Vector==192)ovl_intr=r;else if(r0->u.Interrupt.Vector==194)intr=r;else return fail357(d,231,STATUS_DEVICE_CONFIGURATION_ERROR);irqs++;
   }
  }
 }
 if(d->tracing357){d->start357.words[6]=(ovls&15u)|((rdmas&15u)<<4)|((pwms&15u)<<8)|((posts&15u)<<12);d->start357.words[7]=irqs;}
 if(pwms!=1||posts!=1||!pwm||!post||ovls!=1||rdmas!=1||irqs!=2||!ovl_intr||ovl_intr->ShareDisposition!=CmResourceShareDeviceExclusive||!intr||intr->ShareDisposition!=CmResourceShareDeviceExclusive||(intr->Flags&CM_RESOURCE_INTERRUPT_LATCHED))return fail357(d,232,STATUS_DEVICE_CONFIGURATION_ERROR);
 d->ovl=MmMapIoSpace(ovl->u.Memory.Start,4096,MmNonCached);if(!d->ovl)return fail357(d,240,STATUS_INSUFFICIENT_RESOURCES);
 d->rdma=MmMapIoSpace(rdma->u.Memory.Start,4096,MmNonCached);if(!d->rdma){s=fail357(d,241,STATUS_INSUFFICIENT_RESOURCES);goto failed;}
 if(!a04_post_profile356(sample357(d,OVL_ROI,24),sample357(d,OVL_PITCH0,25),sample357(d,OVL_CON0,26),sample357(d,OVL_SIZE0,27),sample357(d,0x03cu,28))||!sample_addr357(d)||sample357(d,OVL_INTEN,30)||sample_rdma357(d)){s=fail357(d,242,STATUS_DEVICE_CONFIGURATION_ERROR);goto failed;}
 if((ULONGLONG)sample_addr357(d)<(ULONGLONG)post->u.Memory.Start.QuadPart||(ULONGLONG)sample_addr357(d)+FRAME_BYTES>(ULONGLONG)post->u.Memory.Start.QuadPart+post->u.Memory.Length){s=fail357(d,243,STATUS_DEVICE_CONFIGURATION_ERROR);goto failed;}
 RtlZeroMemory(&d->brightness,sizeof(d->brightness));d->brightness.Count=1;
 d->brightness.List[0].InterfaceType=Internal;d->brightness.List[0].PartialResourceList.Version=1;d->brightness.List[0].PartialResourceList.Revision=1;
 d->brightness.List[0].PartialResourceList.Count=1;d->brightness.List[0].PartialResourceList.PartialDescriptors[0]=*pwm;
 for(ULONG i=0;i<27;i++)d->saved[i]=rd(d,owned_offsets356[i]);
 d->old_address=sample_addr357(d);d->old_source=rd(d,OVL_SRC);d->old_con=rd(d,OVL_CON0);d->old_rdma=rd(d,OVL_RDMA0);d->old_pitch=rd(d,OVL_PITCH0);
 RtlZeroMemory(&description,sizeof(description));description.Version=DEVICE_DESCRIPTION_VERSION3;description.Master=TRUE;description.ScatterGather=TRUE;description.DmaAddressWidth=32;description.InterfaceType=Internal;description.MaximumLength=FRAME_BYTES;
 d->dma=IoGetDmaAdapter(d->pdo,&description,&d->map_registers);
 if(d->tracing357){d->start357.words[33]=d->map_registers;d->start357.words[34]=(d->dma?1u:0u)|(d->dma&&d->dma->DmaOperations?2u:0u);if(d->dma&&d->dma->DmaOperations)d->start357.words[34]|=(d->dma->DmaOperations->GetScatterGatherList?4u:0u)|(d->dma->DmaOperations->PutScatterGatherList?8u:0u)|(d->dma->DmaOperations->PutDmaAdapter?16u:0u);}
 if(!d->dma||!d->dma->DmaOperations||!d->dma->DmaOperations->GetScatterGatherList||!d->dma->DmaOperations->PutScatterGatherList||!d->dma->DmaOperations->PutDmaAdapter){s=fail357(d,250,STATUS_DEVICE_CONFIGURATION_ERROR);goto failed;}
 /* Genuine translated interrupt connection, not a timer-generated vblank. */
 s=IoConnectInterrupt(&d->ovl_interrupt,irq,d,NULL,ovl_intr->u.Interrupt.Vector,(KIRQL)ovl_intr->u.Interrupt.Level,(KIRQL)ovl_intr->u.Interrupt.Level,LevelSensitive,FALSE,ovl_intr->u.Interrupt.Affinity,FALSE);
 if(d->tracing357)d->start357.words[35]=(ULONG)s;
 if(!NT_SUCCESS(s)){fail357(d,260,s);goto failed;}
 s=IoConnectInterrupt(&d->interrupt,irq,d,NULL,intr->u.Interrupt.Vector,(KIRQL)intr->u.Interrupt.Level,(KIRQL)intr->u.Interrupt.Level,LevelSensitive,FALSE,intr->u.Interrupt.Affinity,FALSE);
 if(d->tracing357)d->start357.words[36]=(ULONG)s;
 if(!NT_SUCCESS(s)){fail357(d,261,s);goto failed;}
 d->closing=FALSE;d->started=TRUE;s=IoSetDeviceInterfaceState(&d->link,TRUE);if(d->tracing357)d->start357.words[37]=(ULONG)s;if(NT_SUCCESS(s))return s;fail357(d,270,s);
 d->started=FALSE;goto failed;
failed:
 if(d->interrupt){IoDisconnectInterrupt(d->interrupt);d->interrupt=NULL;}
 if(d->ovl_interrupt){IoDisconnectInterrupt(d->ovl_interrupt);d->ovl_interrupt=NULL;}
 if(d->dma&&d->dma->DmaOperations&&d->dma->DmaOperations->PutDmaAdapter){d->dma->DmaOperations->PutDmaAdapter(d->dma);d->dma=NULL;}
 if(d->ovl){MmUnmapIoSpace((void *)d->ovl,4096);d->ovl=NULL;}if(d->rdma){MmUnmapIoSpace((void *)d->rdma,4096);d->rdma=NULL;}return s;
}
static NTSTATUS stop(struct display356 *d){
 if(!d->started)return STATUS_SUCCESS;
 KeWaitForSingleObject(&d->mutex,Executive,KernelMode,FALSE,NULL);
 if(d->notify||d->references||!IsListEmpty(&d->maps)||d->active||d->quarantined){KeReleaseMutex(&d->mutex,FALSE);return STATUS_DEVICE_BUSY;}
 d->closing=TRUE;KeReleaseMutex(&d->mutex,FALSE);IoSetDeviceInterfaceState(&d->link,FALSE);wr(d,OVL_INTEN,0);WRITE_REGISTER_ULONG(d->rdma,0);
 IoDisconnectInterrupt(d->interrupt);d->interrupt=NULL;IoDisconnectInterrupt(d->ovl_interrupt);d->ovl_interrupt=NULL;d->started=FALSE;
 d->dma->DmaOperations->PutDmaAdapter(d->dma);d->dma=NULL;MmUnmapIoSpace((void *)d->ovl,4096);d->ovl=NULL;MmUnmapIoSpace((void *)d->rdma,4096);d->rdma=NULL;return STATUS_SUCCESS;
}
static NTSTATUS pnp(PDEVICE_OBJECT device,PIRP irp){
 struct display356 *d=device->DeviceExtension;PIO_STACK_LOCATION sp=IoGetCurrentIrpStackLocation(irp);NTSTATUS s;
 if(sp->MinorFunction==IRP_MN_START_DEVICE){Geo357Begin(&d->start357,2);d->tracing357=TRUE;s=synchronous(d,irp);d->start357.words[0]=(ULONG)s;if(NT_SUCCESS(s))s=start(d,sp->Parameters.StartDevice.AllocatedResourcesTranslated,sp->Parameters.StartDevice.AllocatedResources);else fail357(d,200,s);d->start357.words[1]=(ULONG)s;d->tracing357=FALSE;Geo357End(&d->start357,s);Geo357Store(&d->start357,1,0);d->last_status=s;return complete(irp,s);}
 if(sp->MinorFunction==IRP_MN_QUERY_INTERFACE&&sp->Parameters.QueryInterface.InterfaceType&&
    RtlCompareMemory(sp->Parameters.QueryInterface.InterfaceType,&A04DisplayTransport356Guid,sizeof(GUID))==sizeof(GUID)){
  struct a04_display_transport_v1 *out=(void *)sp->Parameters.QueryInterface.Interface;
  USHORT version=sp->Parameters.QueryInterface.Version;
  USHORT bytes=version==1?sizeof(*out):version==2?sizeof(struct a04_display_transport_v2):0;
  if(irp->RequestorMode!=KernelMode||!bytes||sp->Parameters.QueryInterface.Size!=bytes||!out||!d->started||d->closing)return complete(irp,STATUS_NOINTERFACE);
  KeWaitForSingleObject(&d->mutex,Executive,KernelMode,FALSE,NULL);
  if(!d->started||d->closing){KeReleaseMutex(&d->mutex,FALSE);return complete(irp,STATUS_NOINTERFACE);}
  RtlZeroMemory(out,bytes);out->Header.Size=bytes;out->Header.Version=version;out->Header.Context=d;out->Header.InterfaceReference=reference;out->Header.InterfaceDereference=dereference;
  out->BrightnessResources=brightness_resources;out->Snapshot=snapshot;out->Map=map_backing;out->Unmap=unmap_backing;out->Select=select_backing;out->Restore=restore;out->Subscribe=subscribe;out->Unsubscribe=unsubscribe;out->Raster=raster;out->EmergencyRestore=emergency_restore;if(version==2)((struct a04_display_transport_v2 *)out)->Timing=panel_timing;reference(d);KeReleaseMutex(&d->mutex,FALSE);return complete(irp,STATUS_SUCCESS);
 }
 if(sp->MinorFunction==IRP_MN_QUERY_STOP_DEVICE||sp->MinorFunction==IRP_MN_QUERY_REMOVE_DEVICE){
  KeWaitForSingleObject(&d->mutex,Executive,KernelMode,FALSE,NULL);
  if(d->references||!IsListEmpty(&d->maps)||d->active||d->notify){KeReleaseMutex(&d->mutex,FALSE);return complete(irp,STATUS_DEVICE_BUSY);}
  /* Close new QUERY_INTERFACE admission before acknowledging QueryStop. */
  d->closing=TRUE;KeReleaseMutex(&d->mutex,FALSE);IoSetDeviceInterfaceState(&d->link,FALSE);return forward(device,irp);
 }
 if(sp->MinorFunction==IRP_MN_CANCEL_STOP_DEVICE||sp->MinorFunction==IRP_MN_CANCEL_REMOVE_DEVICE){
  KeWaitForSingleObject(&d->mutex,Executive,KernelMode,FALSE,NULL);
  BOOLEAN reopen=d->started&&!d->quarantined;if(reopen)d->closing=FALSE;
  KeReleaseMutex(&d->mutex,FALSE);if(reopen)IoSetDeviceInterfaceState(&d->link,TRUE);return forward(device,irp);
 }
 if(sp->MinorFunction==IRP_MN_STOP_DEVICE){s=stop(d);if(!NT_SUCCESS(s))return complete(irp,s);return forward(device,irp);}
 if(sp->MinorFunction==IRP_MN_SURPRISE_REMOVAL){
  /* No late MMIO/free after surprise removal; uncertain scanout stays pinned. */
  d->closing=TRUE;d->quarantined=TRUE;d->started=FALSE;IoSetDeviceInterfaceState(&d->link,FALSE);
  if(d->interrupt){IoDisconnectInterrupt(d->interrupt);d->interrupt=NULL;}if(d->ovl_interrupt){IoDisconnectInterrupt(d->ovl_interrupt);d->ovl_interrupt=NULL;}return forward(device,irp);
 }
 if(sp->MinorFunction==IRP_MN_REMOVE_DEVICE){
  s=stop(d);if(!NT_SUCCESS(s)||d->quarantined||d->references||!IsListEmpty(&d->maps)){d->quarantined=TRUE;return forward(device,irp);}
  IoSetDeviceInterfaceState(&d->link,FALSE);RtlFreeUnicodeString(&d->link);IoSkipCurrentIrpStackLocation(irp);s=IoCallDriver(d->lower,irp);IoDetachDevice(d->lower);IoDeleteDevice(device);return s;
 }
 return forward(device,irp);
}
/* The one optional D0 receipt never holds the power IRP pending. The work
 * item keeps its device object alive; its callback touches only its own copy,
 * no MMIO or provider extension. No stop/remove/power admission uses it. */
struct power357 {PIO_WORKITEM work;struct geo_start357_record record;};
static void power_worker357(PDEVICE_OBJECT device,void *context){struct power357 *p=context;UNREFERENCED_PARAMETER(device);Geo357Store(&p->record,1,1);IoFreeWorkItem(p->work);ExFreePoolWithTag(p,TAG);}
static NTSTATUS power_done357(PDEVICE_OBJECT device,PIRP irp,void *context){
 struct power357 *p=context;UNREFERENCED_PARAMETER(device);p->record.words[3]=(ULONG)irp->IoStatus.Status;p->record.words[4]=irp->PendingReturned;p->record.words[5]=KeGetCurrentIrql();if(!NT_SUCCESS(irp->IoStatus.Status))Geo357Fail(&p->record,300,irp->IoStatus.Status);Geo357End(&p->record,irp->IoStatus.Status);
 if(irp->PendingReturned)IoMarkIrpPending(irp);IoQueueWorkItem(p->work,power_worker357,DelayedWorkQueue,p);return STATUS_SUCCESS;
}
static NTSTATUS power(PDEVICE_OBJECT device,PIRP irp){
 struct display356 *d=device->DeviceExtension;PIO_STACK_LOCATION sp=IoGetCurrentIrpStackLocation(irp);struct power357 *p=NULL;
 if(sp->MinorFunction==IRP_MN_SET_POWER&&sp->Parameters.Power.Type==DevicePowerState&&sp->Parameters.Power.State.DeviceState==PowerDeviceD0&&KeGetCurrentIrql()==PASSIVE_LEVEL&&InterlockedCompareExchange(&d->d0_observed357,1,0)==0){
  p=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*p),TAG);if(p){RtlZeroMemory(p,sizeof(*p));p->work=IoAllocateWorkItem(device);if(!p->work){ExFreePoolWithTag(p,TAG);p=NULL;}else{Geo357Begin(&p->record,3);p->record.words[0]=sp->Parameters.Power.Type;p->record.words[1]=sp->MinorFunction;p->record.words[2]=sp->Parameters.Power.State.DeviceState;}}
 }
 PoStartNextPowerIrp(irp);
 if(p){IoCopyCurrentIrpStackLocationToNext(irp);IoSetCompletionRoutine(irp,power_done357,p,TRUE,TRUE,TRUE);}else IoSkipCurrentIrpStackLocation(irp);
 return PoCallDriver(d->lower,irp);
}

static NTSTATUS create_close(PDEVICE_OBJECT device,PIRP irp){UNREFERENCED_PARAMETER(device);return complete(irp,irp->RequestorMode==KernelMode?STATUS_SUCCESS:STATUS_ACCESS_DENIED);}
static NTSTATUS unsupported(PDEVICE_OBJECT device,PIRP irp){UNREFERENCED_PARAMETER(device);return complete(irp,STATUS_INVALID_DEVICE_REQUEST);}
static NTSTATUS add(PDRIVER_OBJECT driver,PDEVICE_OBJECT pdo){
 PDEVICE_OBJECT device;struct display356 *d;NTSTATUS s=IoCreateDevice(driver,sizeof(*d),NULL,FILE_DEVICE_UNKNOWN,FILE_DEVICE_SECURE_OPEN,FALSE,&device);if(!NT_SUCCESS(s))return s;
 d=device->DeviceExtension;RtlZeroMemory(d,sizeof(*d));d->self=device;d->pdo=pdo;KeInitializeMutex(&d->mutex,0);InitializeListHead(&d->maps);
 d->lower=IoAttachDeviceToDeviceStack(device,pdo);if(!d->lower){IoDeleteDevice(device);return STATUS_NO_SUCH_DEVICE;}
 s=IoRegisterDeviceInterface(pdo,&A04DisplayTransport356Guid,NULL,&d->link);if(!NT_SUCCESS(s)){IoDetachDevice(d->lower);IoDeleteDevice(device);return s;}
 device->Flags|=DO_POWER_PAGABLE;device->Flags&=~DO_DEVICE_INITIALIZING;return STATUS_SUCCESS;
}
static void unload(PDRIVER_OBJECT driver){UNREFERENCED_PARAMETER(driver);}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry){
 UNREFERENCED_PARAMETER(registry);if(!driver)return STATUS_INVALID_PARAMETER;
 for(ULONG i=0;i<=IRP_MJ_MAXIMUM_FUNCTION;i++)driver->MajorFunction[i]=unsupported;
 driver->MajorFunction[IRP_MJ_PNP]=pnp;driver->MajorFunction[IRP_MJ_POWER]=power;driver->MajorFunction[IRP_MJ_CREATE]=create_close;driver->MajorFunction[IRP_MJ_CLOSE]=create_close;
 driver->DriverExtension->AddDevice=add;driver->DriverUnload=unload;return STATUS_SUCCESS;
}
