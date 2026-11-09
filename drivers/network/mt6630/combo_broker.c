/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Linux a04br3_mt6630_core.c: one host/runtime lock, fn1 WLAN, fn2 STP,
 * BT task0 and WMT task4. Independent Windows implementation. All physical
 * I/O remains on the pre-existing NDIS PASSIVE worker. No second lease/map. */
#include "ndis_runtime.h"
#include "windows_nvram_reader.h"
#include "../../bluetooth/mt6630/physical_owner.h"
#include "../../bus/mt8167-shared/hfp_interface.h"
#include "../../bluetooth/mt6630/transport_caps.h"
#include "../../bluetooth/mt6630/start_status.h"
#include "../../bluetooth/mt6630/admission_status.h"
C_ASSERT(sizeof(struct a04_bt_admission_status)==32);
static NDIS_SPIN_LOCK registry_lock;
static NDIS_HANDLE registration;
static struct a04_ndis_runtime *registered;
static struct a04_combo_client_file client_file;
/* Sole NDIS worker writes/reads this receipt. No adapter layout/ABI change. */
static struct a04_bt_start_status start_status;static PFILE_OBJECT start_file;
C_ASSERT(sizeof(struct a04_bt_start_status)==64);
static void start_nt(unsigned stage,unsigned returned,NTSTATUS result)
{
 start_status.last_stage=stage;start_status.entered_mask|=1u<<stage;
 if(returned){start_status.returned_mask|=1u<<stage;if(NT_SUCCESS(result))start_status.succeeded_mask|=1u<<stage;}
 start_status.broker_ntstatus=(ULONG)result;
}
static void start_backend(void *unused,unsigned stage,unsigned returned,int result,unsigned opcode)
{
 UNREFERENCED_PARAMETER(unused);start_status.last_stage=stage;start_status.last_opcode=opcode;
 start_status.entered_mask|=1u<<stage;
 if(returned){start_status.returned_mask|=1u<<stage;if(!result)start_status.succeeded_mask|=1u<<stage;start_status.backend_status=result;start_status.flags|=A04_BT_START_BACKEND_VALID;}
}

static NTSTATUS finish(PIRP irp,NTSTATUS s,ULONG_PTR bytes)
{irp->IoStatus.Status=s;irp->IoStatus.Information=bytes;IoCompleteRequest(irp,IO_NO_INCREMENT);return s;}
static NTSTATUS dispatch(PDEVICE_OBJECT device,PIRP irp)
{
 PIO_STACK_LOCATION sp=IoGetCurrentIrpStackLocation(irp);NTSTATUS s;
 UNREFERENCED_PARAMETER(device);
 if(irp->RequestorMode!=KernelMode)return finish(irp,STATUS_ACCESS_DENIED,0);
 if(sp->MajorFunction!=IRP_MJ_CREATE&&sp->MajorFunction!=IRP_MJ_CLOSE&&
    sp->MajorFunction!=IRP_MJ_DEVICE_CONTROL&&sp->MajorFunction!=IRP_MJ_CLEANUP)return finish(irp,STATUS_INVALID_DEVICE_REQUEST,0);
 NdisAcquireSpinLock(&registry_lock);
 if(sp->MajorFunction==IRP_MJ_CREATE)
  s=a04_combo_client_create(&client_file,sp->FileObject)?STATUS_SHARING_VIOLATION:STATUS_SUCCESS;
 else if(sp->MajorFunction==IRP_MJ_CLOSE){a04_combo_client_closed(&client_file,sp->FileObject);s=STATUS_SUCCESS;}
 else if(sp->MajorFunction==IRP_MJ_CLEANUP){
  int first=a04_combo_client_cleanup(&client_file,sp->FileObject);
  /* Once unpublished, adapter Halt owns the joined physical teardown.
   * File cleanup success here is software rundown, not a WMT OFF receipt. */
  s=first<0?STATUS_INVALID_HANDLE:(!first||!registered?STATUS_SUCCESS:a04_ndis_combo_enqueue(registered,irp));
 }else if(!a04_combo_client_controls(&client_file,sp->FileObject))s=STATUS_FILE_CLOSED;
 else s=registered?a04_ndis_combo_enqueue(registered,irp):STATUS_DEVICE_NOT_READY;
 NdisReleaseSpinLock(&registry_lock);
 return s==STATUS_PENDING?s:finish(irp,s,0);
}
NTSTATUS a04_combo_create(NDIS_HANDLE driver)
{
 NDIS_DEVICE_OBJECT_ATTRIBUTES attr;PDEVICE_OBJECT device;PDRIVER_DISPATCH handlers[IRP_MJ_MAXIMUM_FUNCTION+1]={0};
 UNICODE_STRING name=RTL_CONSTANT_STRING(A04_BT_BROKER_NAME),link=RTL_CONSTANT_STRING(A04_BT_BROKER_LINK);
 UNICODE_STRING acl=RTL_CONSTANT_STRING(L"D:P(A;;GA;;;SY)");
 NdisAllocateSpinLock(&registry_lock);handlers[IRP_MJ_CREATE]=dispatch;handlers[IRP_MJ_CLOSE]=dispatch;
 handlers[IRP_MJ_CLEANUP]=dispatch;handlers[IRP_MJ_DEVICE_CONTROL]=dispatch;
 NdisZeroMemory(&attr,sizeof(attr));attr.Header.Type=NDIS_OBJECT_TYPE_DEVICE_OBJECT_ATTRIBUTES;
 attr.Header.Revision=NDIS_DEVICE_OBJECT_ATTRIBUTES_REVISION_1;attr.Header.Size=sizeof(attr);
 attr.DeviceName=&name;attr.SymbolicName=&link;attr.MajorFunctions=handlers;attr.DefaultSDDLString=&acl;
 NTSTATUS s=NdisRegisterDeviceEx(driver,&attr,&device,&registration);
 if(!NT_SUCCESS(s)){registration=NULL;NdisFreeSpinLock(&registry_lock);}return s;
}
void a04_combo_delete(void)
{if(registration){NdisDeregisterDeviceEx(registration);registration=NULL;NdisFreeSpinLock(&registry_lock);}}
int a04_combo_publish(struct a04_ndis_runtime *a)
{int result=-1;NdisAcquireSpinLock(&registry_lock);if(!registered){registered=a;result=0;}NdisReleaseSpinLock(&registry_lock);return result;}
void a04_combo_unpublish(struct a04_ndis_runtime *a)
{NdisAcquireSpinLock(&registry_lock);if(registered==a)registered=NULL;NdisReleaseSpinLock(&registry_lock);}
/* Query only the real NDIS PDO. Root client is independently enumerated and
 * must never cast an opaque parent context to a WDF object to find hardware. */
static NTSTATUS owner_query_complete(PDEVICE_OBJECT device,PIRP irp,PVOID event)
{UNREFERENCED_PARAMETER(device);UNREFERENCED_PARAMETER(irp);KeSetEvent(event,IO_NO_INCREMENT,FALSE);return STATUS_MORE_PROCESSING_REQUIRED;}
static NTSTATUS query_physical_owner(struct a04_ndis_runtime *a,struct a04_bt_physical_owner *owner)
{
 PDEVICE_OBJECT pdo=NULL;PIRP irp;PIO_STACK_LOCATION sp;KEVENT event;NTSTATUS status;struct A04_SHARED_HFP_INTERFACE hfp={0};
 NdisMGetDeviceProperty(a->adapter,&pdo,NULL,NULL,NULL,NULL);if(!pdo)return STATUS_NO_SUCH_DEVICE;
 KeInitializeEvent(&event,NotificationEvent,FALSE);irp=IoAllocateIrp(pdo->StackSize,FALSE);if(!irp)return STATUS_INSUFFICIENT_RESOURCES;
 sp=IoGetNextIrpStackLocation(irp);sp->MajorFunction=IRP_MJ_PNP;sp->MinorFunction=IRP_MN_QUERY_INTERFACE;
 sp->Parameters.QueryInterface.InterfaceType=&A04SharedHfpInterfaceGuid;sp->Parameters.QueryInterface.Size=sizeof(hfp);
 sp->Parameters.QueryInterface.Version=A04_SHARED_HFP_VERSION;sp->Parameters.QueryInterface.Interface=(PINTERFACE)&hfp;
 irp->IoStatus.Status=STATUS_NOT_SUPPORTED;IoSetCompletionRoutine(irp,owner_query_complete,&event,TRUE,TRUE,TRUE);
 status=IoCallDriver(pdo,irp);if(status==STATUS_PENDING)KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);
 status=irp->IoStatus.Status;IoFreeIrp(irp);
 if(NT_SUCCESS(status)){
  if(hfp.Header.Size!=sizeof(hfp)||hfp.Header.Version!=A04_SHARED_HFP_VERSION||!hfp.Header.Context||
     !hfp.Header.InterfaceReference||!hfp.Header.InterfaceDereference||!hfp.ControllerEpochLive||
     hfp.ControllerEpochLive(hfp.Header.Context,a->session.generation)!=1)status=STATUS_NOT_SUPPORTED;
  else {owner->Header=hfp.Header;owner->Header.Size=sizeof(*owner);owner->Header.Version=A04_BT_PHYSICAL_OWNER_VERSION;
   owner->generation=a->session.generation;owner->Live=hfp.ControllerEpochLive;return STATUS_SUCCESS;}
 }
 if(hfp.Header.Context&&hfp.Header.InterfaceDereference)hfp.Header.InterfaceDereference(hfp.Header.Context);return status;
}
static uint64_t clock_us(void *p){UNREFERENCED_PARAMETER(p);return KeQueryInterruptTime()/10u;}
static int receive(void *p,unsigned type,const uint8_t *data,unsigned n)
{
 struct a04_combo_broker *b=p;if(b->count==A04_COMBO_RX_SLOTS||n>A04_BT_BROKER_MAX_HCI)return -12;
 struct a04_bt_broker_packet *r=&b->rx[b->count++];r->version=1;r->type=type;r->length=n;RtlCopyMemory(r->data,data,n);return 0;
}
int a04_combo_poll(struct a04_ndis_runtime *a)
{
 if(!a->combo.file)return 0;
 if(a04_bt_poll(&a->combo.bt)){a->radio.failed=-1;return -1;}return 0;
}
static int service(void *p){return a04_combo_poll(p);}
int a04_combo_held(struct a04_ndis_runtime *a){return a->combo.sco_holds!=0;}
int a04_combo_stop(struct a04_ndis_runtime *a,int removed)
{
 struct a04_combo_broker *b=&a->combo;int result=b->bt.failed?-1:0;
 /* A real Audio controller hold forbids physical shutdown. Halt/removal
  * retains the existing quarantined session instead of certifying DMA drain. */
 if(b->sco_holds)return -1;
 if(a->radio.bridge){a->radio.bridge->service_other=NULL;a->radio.bridge->service_context=NULL;}
 if(b->file){
  /* Before lifetime STOPPING. Never reset/PMU-off a combo from a BT close. */
  if(removed||a04_bt_disable(&b->bt))result=-1;
  {PFILE_OBJECT file=b->file;b->file=NULL;ObDereferenceObject(file);}
 }
 RtlSecureZeroMemory(b->rx,sizeof(b->rx));b->count=0;
 /* The caller is revoking the hardware epoch; no stale initialized channel
  * can be reused after the session's cold reset/removal. */
 if(!result||removed)RtlSecureZeroMemory(&b->bt,sizeof(b->bt));return result;
}
void a04_combo_worker(struct a04_ndis_runtime *a)
{
 struct a04_combo_broker *b=&a->combo;PIRP irp;unsigned cleanup;NTSTATUS s=STATUS_INVALID_DEVICE_REQUEST;ULONG_PTR used=0;
 NdisAcquireSpinLock(&a->lock);irp=a04_combo_requests_take(&b->requests,&cleanup);NdisReleaseSpinLock(&a->lock);
 if(!irp)return;
 PIO_STACK_LOCATION sp=IoGetCurrentIrpStackLocation(irp);ULONG code=sp->Parameters.DeviceIoControl.IoControlCode;
 ULONG in=sp->Parameters.DeviceIoControl.InputBufferLength,out=sp->Parameters.DeviceIoControl.OutputBufferLength;
 if(cleanup){code=A04_BT_BROKER_CLOSE;in=out=0;}
 if(!sp->FileObject){finish(irp,STATUS_INVALID_HANDLE,0);return;}
 if(a->closing){finish(irp,cleanup?STATUS_SUCCESS:STATUS_CANCELLED,0);return;}
 /* CLEANUP is mandatory rundown, not a cancellable HCI operation. */
 if(!cleanup&&irp->Cancel){finish(irp,STATUS_CANCELLED,0);return;}
 if(code==A04_BT_BROKER_ADMISSION){
  struct a04_bt_admission_status *state=irp->AssociatedIrp.SystemBuffer;
  if(in||out!=sizeof(*state)||!state){s=STATUS_INVALID_PARAMETER;goto done;}
  /* Single existing worker; observes software state before OPEN, no SDIO,
   * WMT, owner acquire or power transition and no invented HCI readiness. */
  RtlZeroMemory(state,sizeof(*state));state->version=1;state->bytes=sizeof(*state);
  state->generation=a->session.generation;state->power=!!a->power;
  state->initialized=!!a->radio.initialized;state->failed=!!a->radio.failed;state->uncertain=!!a->uncertain;
  state->ready=state->generation&&state->power&&state->initialized&&!state->failed&&!state->uncertain;
  used=sizeof(*state);s=STATUS_SUCCESS;
 }else if(code==A04_BT_BROKER_START_STATUS){
  struct a04_bt_start_status *out_status=irp->AssociatedIrp.SystemBuffer;
  if(in||out!=sizeof(*out_status)||!out_status){s=STATUS_INVALID_PARAMETER;goto done;}
  if(start_file!=sp->FileObject||start_status.generation!=a->session.generation||!(start_status.flags&A04_BT_START_RETURNED)){s=STATUS_DEVICE_NOT_READY;goto done;}
  RtlCopyMemory(out_status,&start_status,sizeof(*out_status));used=sizeof(*out_status);s=STATUS_SUCCESS;
 }else if(code==A04_BT_BROKER_OPEN){
  UCHAR record[36]={0};int backend;
  if(in||out){s=STATUS_INVALID_PARAMETER;goto done;}
  if(b->file){s=STATUS_SHARING_VIOLATION;goto done;}
  ULONG sequence=start_status.sequence==MAXULONG?MAXULONG:start_status.sequence+1;
  RtlZeroMemory(&start_status,sizeof(start_status));start_status.version=1;start_status.bytes=sizeof(start_status);start_status.generation=a->session.generation;start_status.sequence=sequence;start_file=sp->FileObject;
  {LARGE_INTEGER wall;ULONGLONG ticks=KeQueryInterruptTime();KeQuerySystemTime(&wall);
   start_status.start_filetime_low=wall.LowPart;start_status.start_filetime_high=(ULONG)wall.HighPart;
   start_status.start_interrupttime_low=(ULONG)ticks;start_status.start_interrupttime_high=(ULONG)(ticks>>32);}
  start_nt(A04_BT_START_ADMISSION,0,STATUS_PENDING);
  if(!a->power||!a->radio.initialized||a->radio.failed||a->uncertain){s=STATUS_DEVICE_NOT_READY;start_nt(A04_BT_START_ADMISSION,1,s);goto open_done;}
  start_nt(A04_BT_START_ADMISSION,1,STATUS_SUCCESS);start_nt(A04_BT_START_NVRAM,0,STATUS_PENDING);
  s=a04_windows_bt_nvram_read(record);start_nt(A04_BT_START_NVRAM,1,s);
  if(NT_SUCCESS(s)){
   start_backend(NULL,A04_BT_START_ATTACH,0,0,0);backend=a04_bt_open_combo(&b->bt,a->radio.bridge,b,receive,clock_us);start_backend(NULL,A04_BT_START_ATTACH,1,backend,0);
   if(!backend)backend=a04_bt_initialize_observed(&b->bt,record,start_backend,NULL);
   if(backend)s=STATUS_IO_DEVICE_ERROR;
   else {b->file=sp->FileObject;ObReferenceObject(b->file);
    a->radio.bridge->service_context=a;a->radio.bridge->service_other=service;s=STATUS_SUCCESS;}
  }
open_done:
  RtlSecureZeroMemory(record,sizeof(record));start_status.broker_ntstatus=(ULONG)s;start_status.flags|=A04_BT_START_RETURNED|A04_BT_START_NT_VALID;
 }else if(!b->file||b->file!=sp->FileObject){s=cleanup?STATUS_SUCCESS:STATUS_DEVICE_NOT_READY;}
 else if(code==A04_BT_BROKER_CLOSE){
  if(in||out){s=STATUS_INVALID_PARAMETER;goto done;}
  if(b->sco_holds){s=STATUS_DEVICE_BUSY;goto done;}
  a->radio.bridge->service_other=NULL;a->radio.bridge->service_context=NULL;
  s=a04_bt_disable(&b->bt)?STATUS_IO_DEVICE_ERROR:STATUS_SUCCESS;
  /* Keep attachment metadata after a clean OFF for reopening this fn2 owner.
   * A failure keeps bt.failed/bridge.failed and cannot create a new lease. */
  {PFILE_OBJECT file=b->file;b->file=NULL;ObDereferenceObject(file);}RtlSecureZeroMemory(b->rx,sizeof(b->rx));b->count=0;
 }else if(code==A04_BT_BROKER_GET_STATE){
  struct a04_bt_broker_state *state=irp->AssociatedIrp.SystemBuffer;
  if(in||out!=sizeof(*state)||!state){s=STATUS_INVALID_PARAMETER;goto done;}
  RtlZeroMemory(state,sizeof(*state));state->version=2;
  state->ready=a->power&&a->radio.initialized&&!a->radio.failed&&!a->uncertain&&b->bt.ready&&!b->bt.failed&&
   a->session.generation&&a->parent.Live(a->parent.Header.Context,a->session.generation)==1;
  state->pcm_command_applied=state->ready&&b->bt.pcm_command_applied;
  state->generation=a->session.generation;
  used=sizeof(*state);s=STATUS_SUCCESS;
 }else if(code==A04_BT_BROKER_TRANSPORT_CAPS){
  struct a04_bt_transport_caps *caps=irp->AssociatedIrp.SystemBuffer;
  struct a04_wmt_bridge *bridge=a->radio.bridge;
  if(in||out!=sizeof(*caps)||!caps){s=STATUS_INVALID_PARAMETER;goto done;}
  RtlZeroMemory(caps,sizeof(*caps));caps->version=1;
  /* Same profile passes MERGE_REQUIRED to the real startup command sequence.
   * READY is reached only after its ACK and identity checks. FC72 ACK is a
   * routing/configuration fact, not decoded sample format/latency proof. */
  struct a04_mt6630_route_facts facts={0};
  if(bridge){
   facts.merge=A04_BT_COMBO_MERGE_REQUIRED;facts.firmware_ready=bridge->wmt_ready;
   facts.firmware_fault=bridge->failed||bridge->wmt_state.failed;
   facts.stp_ready=bridge->wmt_state.stp_ready;facts.phase_ready=bridge->wmt_state.phase==A04_WMT_DL_READY;
   facts.commands=bridge->wmt_state.commands;facts.acked=bridge->wmt_state.acked;
   facts.radio_ready=a->power&&a->radio.initialized&&!a->radio.failed&&!a->uncertain&&b->bt.ready&&!b->bt.failed;
   facts.version_valid=b->bt.version_valid;facts.pcm_ack=b->bt.pcm_command_applied;
   facts.epoch_live=a->session.generation&&a->parent.Live(a->parent.Header.Context,a->session.generation)==1;
  }
  if(a04_mt6630_route_ready(&facts)){
   caps->ready=1;caps->generation=a->session.generation;caps->routes=A04_BT_ROUTE_HCI_BYPASS;
  }
  used=sizeof(*caps);s=STATUS_SUCCESS;
 }else if(code==A04_BT_BROKER_GET_PHYSICAL_OWNER){
  struct a04_bt_physical_owner *owner=irp->UserBuffer;
  if(in||out!=sizeof(*owner)||!owner){s=STATUS_INVALID_PARAMETER;goto done;}
  if(!a->parent.Header.InterfaceReference||!a->parent.Header.InterfaceDereference||!a->parent.Live||
     !a->session.generation||a->parent.Live(a->parent.Header.Context,a->session.generation)!=1){s=STATUS_DEVICE_NOT_READY;goto done;}
  /* Direct kernel output remains visible even if a late cancellation changes
   * completion status. Caller dereferences any returned interface on failure. */
  RtlZeroMemory(owner,sizeof(*owner));s=query_physical_owner(a,owner);
  if(NT_SUCCESS(s))used=sizeof(*owner);
 }else if(code==A04_BT_BROKER_SCO_HOLD){
  struct a04_bt_broker_state *state=irp->AssociatedIrp.SystemBuffer;
  if(in||out!=sizeof(*state)||!state){s=STATUS_INVALID_PARAMETER;goto done;}
  if(!a->power||a->radio.failed||a->uncertain||!b->bt.ready||!b->bt.pcm_command_applied||
     !a->session.generation||a->parent.Live(a->parent.Header.Context,a->session.generation)!=1){s=STATUS_DEVICE_NOT_READY;goto done;}
  if(b->sco_holds==MAXULONG){s=STATUS_DEVICE_BUSY;goto done;}
  b->sco_holds++;RtlZeroMemory(state,sizeof(*state));state->version=2;state->ready=1;
  state->pcm_command_applied=1;state->generation=a->session.generation;used=sizeof(*state);s=STATUS_SUCCESS;
 }else if(code==A04_BT_BROKER_SCO_RELEASE){
  ULONG *generation=irp->AssociatedIrp.SystemBuffer;
  if(in!=sizeof(*generation)||out||!generation||!*generation||*generation!=a->session.generation||!b->sco_holds){s=STATUS_INVALID_PARAMETER;goto done;}
  b->sco_holds--;s=STATUS_SUCCESS;
 }else if(code==A04_BT_BROKER_WRITE){
  struct a04_bt_broker_packet *packet=irp->AssociatedIrp.SystemBuffer;
  if(in!=sizeof(*packet)||out||!packet||packet->version!=1||packet->length>sizeof(packet->data)){s=STATUS_INVALID_PARAMETER;goto done;}
  s=a04_bt_write(&b->bt,packet->type,packet->data,packet->length)?STATUS_IO_DEVICE_ERROR:STATUS_SUCCESS;
  RtlSecureZeroMemory(packet,sizeof(*packet));
 }else if(code==A04_BT_BROKER_READ){
  if(in||out!=sizeof(struct a04_bt_broker_packet)||!irp->AssociatedIrp.SystemBuffer){s=STATUS_INVALID_PARAMETER;goto done;}
  if(!b->count){s=STATUS_NO_MORE_ENTRIES;goto done;}
  RtlCopyMemory(irp->AssociatedIrp.SystemBuffer,&b->rx[0],sizeof(b->rx[0]));used=sizeof(b->rx[0]);
  for(unsigned i=1;i<b->count;i++)b->rx[i-1]=b->rx[i];
  RtlSecureZeroMemory(&b->rx[--b->count],sizeof(b->rx[0]));s=STATUS_SUCCESS;
 }
done:
 if(cleanup&&start_file==sp->FileObject)start_file=NULL;
 if(b->bt.failed){a->radio.failed=-1;/* preserve the real combo fault, never report WLAN healthy */}
 (void)finish(irp,s,used);
}

/* Called after admission is revoked and timer/DPCs joined. If allocation of
 * a new work item failed, pending CLEANUP is still owned by this adapter.
 * With no worker active, finish software requests before Halt waits idle.
 * The joined caller subsequently performs a04_combo_stop while I/O is live;
 * no physical OFF success is invented by completing a file-cleanup IRP. */
void a04_combo_finish_queued(struct a04_ndis_runtime *a)
{
 PIRP requests[2]={NULL,NULL};unsigned kinds[2]={0,0};
 NdisAcquireSpinLock(&a->lock);
 if(a->closing&&!a->busy){
  for(unsigned i=0;i<2;i++)requests[i]=a04_combo_requests_take(&a->combo.requests,&kinds[i]);
  if(!a->api_refs)NdisSetEvent(&a->idle);
 }
 NdisReleaseSpinLock(&a->lock);
 for(unsigned i=0;i<2;i++)if(requests[i])finish(requests[i],kinds[i]?STATUS_SUCCESS:STATUS_CANCELLED,0);
}
