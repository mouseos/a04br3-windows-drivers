/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Linux a04br3_mt6630_core.c: one host/runtime lock, fn1 WLAN, fn2 STP,
 * BT task0 and WMT task4. Independent Windows implementation. All physical
 * I/O remains on the pre-existing NDIS PASSIVE worker. No second lease/map. */
#include "ndis_runtime.h"
#include "windows_nvram_reader.h"
static NDIS_SPIN_LOCK registry_lock;
static NDIS_HANDLE registration;
static struct a04_ndis_runtime *registered;
static NTSTATUS finish(PIRP irp,NTSTATUS s,ULONG_PTR bytes)
{irp->IoStatus.Status=s;irp->IoStatus.Information=bytes;IoCompleteRequest(irp,IO_NO_INCREMENT);return s;}
static NTSTATUS dispatch(PDEVICE_OBJECT device,PIRP irp)
{
 PIO_STACK_LOCATION sp=IoGetCurrentIrpStackLocation(irp);NTSTATUS s;
 UNREFERENCED_PARAMETER(device);
 if(irp->RequestorMode!=KernelMode)return finish(irp,STATUS_ACCESS_DENIED,0);
 if(sp->MajorFunction==IRP_MJ_CREATE||sp->MajorFunction==IRP_MJ_CLOSE)return finish(irp,STATUS_SUCCESS,0);
 if(sp->MajorFunction!=IRP_MJ_DEVICE_CONTROL&&sp->MajorFunction!=IRP_MJ_CLEANUP)return finish(irp,STATUS_INVALID_DEVICE_REQUEST,0);
 NdisAcquireSpinLock(&registry_lock);
 s=registered?a04_ndis_combo_enqueue(registered,irp):STATUS_DEVICE_NOT_READY;
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
int a04_combo_stop(struct a04_ndis_runtime *a,int removed)
{
 struct a04_combo_broker *b=&a->combo;int result=b->bt.failed?-1:0;
 if(a->radio.bridge){a->radio.bridge->service_other=NULL;a->radio.bridge->service_context=NULL;}
 if(b->file){
  /* Before lifetime STOPPING. Never reset/PMU-off a combo from a BT close. */
  if(removed||a04_bt_disable(&b->bt))result=-1;
  ObDereferenceObject(b->file);b->file=NULL;
 }
 RtlSecureZeroMemory(b->rx,sizeof(b->rx));b->count=0;
 /* The caller is revoking the hardware epoch; no stale initialized channel
  * can be reused after the session's cold reset/removal. */
 if(!result||removed)RtlSecureZeroMemory(&b->bt,sizeof(b->bt));return result;
}
void a04_combo_worker(struct a04_ndis_runtime *a)
{
 struct a04_combo_broker *b=&a->combo;PIRP irp;NTSTATUS s=STATUS_INVALID_DEVICE_REQUEST;ULONG_PTR used=0;
 NdisAcquireSpinLock(&a->lock);irp=b->request;b->request=NULL;NdisReleaseSpinLock(&a->lock);
 if(!irp)return;
 PIO_STACK_LOCATION sp=IoGetCurrentIrpStackLocation(irp);ULONG code=sp->Parameters.DeviceIoControl.IoControlCode;
 ULONG in=sp->Parameters.DeviceIoControl.InputBufferLength,out=sp->Parameters.DeviceIoControl.OutputBufferLength;
 if(sp->MajorFunction==IRP_MJ_CLEANUP){code=A04_BT_BROKER_CLOSE;in=out=0;}
 if(!sp->FileObject){finish(irp,STATUS_INVALID_HANDLE,0);return;}
 if(a->closing||irp->Cancel){finish(irp,STATUS_CANCELLED,0);return;}
 if(code==A04_BT_BROKER_OPEN){
  UCHAR record[36]={0};
  if(in||out){s=STATUS_INVALID_PARAMETER;goto done;}
  if(b->file){s=STATUS_SHARING_VIOLATION;goto done;}
  if(!a->power||!a->radio.initialized||a->radio.failed||a->uncertain){s=STATUS_DEVICE_NOT_READY;goto done;}
  s=a04_windows_bt_nvram_read(record);
  if(NT_SUCCESS(s)){
   if(a04_bt_open_combo(&b->bt,a->radio.bridge,b,receive,clock_us)||a04_bt_initialize(&b->bt,record))s=STATUS_IO_DEVICE_ERROR;
   else {b->file=sp->FileObject;ObReferenceObject(b->file);
    a->radio.bridge->service_context=a;a->radio.bridge->service_other=service;s=STATUS_SUCCESS;}
  }
  RtlSecureZeroMemory(record,sizeof(record));
 }else if(!b->file||b->file!=sp->FileObject){s=STATUS_DEVICE_NOT_READY;}
 else if(code==A04_BT_BROKER_CLOSE){
  if(in||out){s=STATUS_INVALID_PARAMETER;goto done;}
  a->radio.bridge->service_other=NULL;a->radio.bridge->service_context=NULL;
  s=a04_bt_disable(&b->bt)?STATUS_IO_DEVICE_ERROR:STATUS_SUCCESS;
  /* Keep attachment metadata after a clean OFF for reopening this fn2 owner.
   * A failure keeps bt.failed/bridge.failed and cannot create a new lease. */
  ObDereferenceObject(b->file);b->file=NULL;RtlSecureZeroMemory(b->rx,sizeof(b->rx));b->count=0;
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
 if(b->bt.failed){a->radio.failed=-1;/* preserve the real combo fault, never report WLAN healthy */}
 (void)finish(irp,s,used);
}
