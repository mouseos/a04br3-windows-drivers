/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native SDIO/STP transport. No UART and no second MSDC2 owner. */
#include <ntddk.h>
#include <wdf.h>
#include <bthxddi.h>
#include "transport.h"
#include "../../network/mt6630/windows_board.h"
#include "../../network/mt6630/windows_irq.h"
#include "../../network/mt6630/windows_firmware.h"
#include "../../network/mt6630/windows_nvram_reader.h"
#include "../../network/mt6630/sha256.h"
#include "../../network/mt6630/wdf_hardware_config.h"
#define BT_SLOTS 32
#define BT_CONTEXT_BYTES FIELD_OFFSET(BTHX_HCI_READ_WRITE_CONTEXT,Data)
struct bt_packet {unsigned type,length;uint8_t data[A04_BT_MAX_HCI];};
typedef struct _BT_DEVICE {
 WDFDEVICE device,child;WDFWAITLOCK lock;WDFTIMER timer;WDFQUEUE reads[2];
 A04_SHARED_WIFI_INTERFACE parent;ULONG generation;int referenced,leased,uncertain,removed,running,negotiated;
 CM_PARTIAL_RESOURCE_DESCRIPTOR mem,trans_mem,irq_resource,trans_irq;
 struct a04_wifi_owner owner;struct a04_windows_msdc2 host;struct a04_windows_irq irq;
 struct a04_windows_wifi_board board_io;struct a04_wifi_board board;struct a04_wifi_life life;
 struct a04_sdio_card_info card;struct a04_wifi_life_ops ops;struct a04_wmt_bridge bridge;
 struct a04_bt_transport bt;struct bt_packet packets[BT_SLOTS];unsigned packet_count;
} BT_DEVICE;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BT_DEVICE,BtContext)
EVT_WDF_DRIVER_DEVICE_ADD BtAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE BtPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE BtRelease;
EVT_WDF_DEVICE_D0_ENTRY BtStart;
EVT_WDF_DEVICE_D0_EXIT BtStop;
EVT_WDF_DEVICE_SURPRISE_REMOVAL BtRemoved;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL BtInternal;
EVT_WDF_TIMER BtTimer;
static void lock(BT_DEVICE *c){(void)WdfWaitLockAcquire(c->lock,NULL);}
static void unlock(BT_DEVICE *c){WdfWaitLockRelease(c->lock);}
static void notify(void *p){UNREFERENCED_PARAMETER(p);} /* polling, INTEN=0 */
static int mask(void *p){BT_DEVICE *c=p;return c->irq.interrupt?a04_windows_irq_stop(&c->irq):0;}
static int drain(void *p)
{
 BT_DEVICE *c=p;
 if(c->removed||a04_bt_disable(&c->bt))return -1;
 return a04_wifi_board_poweroff_signals(&c->board,&c->board_io.io);
}
static int unmap(void *p){BT_DEVICE *c=p;a04_windows_msdc2_unmap(&c->host);return 0;}
static int release(void *p)
{
 BT_DEVICE *c=p;
 if(!NT_SUCCESS(c->parent.ReleaseQuiescent(c->parent.Header.Context,c->generation)))return -1;
 c->leased=0;return 0;
}
static int receive(void *p,unsigned type,const uint8_t *data,unsigned length)
{
 BT_DEVICE *c=p;struct bt_packet *packet;
 if(c->packet_count==BT_SLOTS||length>A04_BT_MAX_HCI)return -12;
 packet=&c->packets[c->packet_count++];packet->type=type;packet->length=length;
 RtlCopyMemory(packet->data,data,length);return 0;
}
/* Queue-owned requests remain cancelable. Retrieve transfers ownership before
 * touching kernel METHOD_NEITHER buffers; no raw IRP is retained across PnP. */
static void deliver(BT_DEVICE *c)
{
 for(unsigned i=0;i<c->packet_count;){
  struct bt_packet *p=&c->packets[i];WDFREQUEST request;PVOID out;size_t bytes;NTSTATUS status;
  if(!NT_SUCCESS(WdfIoQueueRetrieveNextRequest(c->reads[p->type==4?0:1],&request))){i++;continue;}
  status=WdfRequestRetrieveOutputBuffer(request,BT_CONTEXT_BYTES+p->length,&out,&bytes);
  if(!NT_SUCCESS(status)){WdfRequestComplete(request,status);continue;}
  PBTHX_HCI_READ_WRITE_CONTEXT h=out;h->Type=(UCHAR)p->type;h->DataLen=p->length;
  RtlCopyMemory(h->Data,p->data,p->length);
  WdfRequestCompleteWithInformation(request,STATUS_SUCCESS,BT_CONTEXT_BYTES+p->length);
  RtlSecureZeroMemory(p,sizeof(*p));
  for(unsigned j=i+1;j<c->packet_count;j++)c->packets[j-1]=c->packets[j];
  c->packet_count--;RtlSecureZeroMemory(&c->packets[c->packet_count],sizeof(*p));
 }
}
VOID BtTimer(WDFTIMER timer)
{
 BT_DEVICE *c=BtContext(WdfTimerGetParentObject(timer));lock(c);
 if(c->running){
  if(a04_bt_poll(&c->bt)){
   c->running=0;c->uncertain=1;
   DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"MT6630BT: transport failed status=%d\n",c->bt.failed);
   WdfIoQueuePurge(c->reads[0],NULL,NULL);WdfIoQueuePurge(c->reads[1],NULL,NULL);
  }else {deliver(c);WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(5));}
 }
 unlock(c);
}
VOID BtInternal(WDFQUEUE queue,WDFREQUEST request,size_t outlen,size_t inlen,ULONG code)
{
 WDFDEVICE child=WdfIoQueueGetDevice(queue);BT_DEVICE *c=BtContext(WdfPdoGetParent(child));
 PVOID in=NULL,out=NULL;size_t length;NTSTATUS status=STATUS_INVALID_PARAMETER;ULONG_PTR used=0;
 UNREFERENCED_PARAMETER(inlen);UNREFERENCED_PARAMETER(outlen);
 if(WdfRequestGetRequestorMode(request)!=KernelMode){WdfRequestComplete(request,STATUS_ACCESS_DENIED);return;}
 lock(c);
 switch(code){
 case IOCTL_BTHX_GET_VERSION:
  status=WdfRequestRetrieveOutputBuffer(request,sizeof(BTHX_VERSION),&out,&length);
  if(NT_SUCCESS(status)){((PBTHX_VERSION)out)->Version=BTHX_DDI_VERSION_1;used=sizeof(BTHX_VERSION);}break;
 case IOCTL_BTHX_SET_VERSION:
  status=WdfRequestRetrieveInputBuffer(request,sizeof(BTHX_VERSION),&in,&length);
  if(NT_SUCCESS(status)){status=((PBTHX_VERSION)in)->Version==BTHX_DDI_VERSION_1?STATUS_SUCCESS:STATUS_REVISION_MISMATCH;c->negotiated=NT_SUCCESS(status);}break;
 case IOCTL_BTHX_QUERY_CAPABILITIES:
  status=WdfRequestRetrieveOutputBuffer(request,sizeof(BTHX_CAPABILITIES),&out,&length);
  if(NT_SUCCESS(status)){
   PBTHX_CAPABILITIES caps=out;RtlZeroMemory(caps,sizeof(*caps));caps->MaxAclTransferInSize=A04_BT_MAX_HCI;
   /* SCO wiring and alternate audio path have not been established. Do not
    * advertise HCIBypass just to make BthMini accept this controller. */
   caps->ScoSupport=ScoSupportNone;used=sizeof(*caps);
  }break;
 case IOCTL_BTHX_WRITE_HCI:
  if(!c->running||!c->negotiated){status=STATUS_DEVICE_NOT_READY;break;}
  status=WdfRequestRetrieveOutputBuffer(request,sizeof(ULONG),&out,&length);if(!NT_SUCCESS(status))break;
  status=WdfRequestRetrieveInputBuffer(request,BT_CONTEXT_BYTES+1,&in,&length);if(!NT_SUCCESS(status))break;
  {PBTHX_HCI_READ_WRITE_CONTEXT h=in;
   if(h->DataLen>length-BT_CONTEXT_BYTES||h->DataLen>A04_BT_MAX_HCI){status=STATUS_INVALID_BUFFER_SIZE;break;}
   status=a04_bt_write(&c->bt,h->Type,h->Data,h->DataLen)?STATUS_IO_DEVICE_ERROR:STATUS_SUCCESS;
   if(NT_SUCCESS(status)){*(ULONG *)out=h->DataLen;used=sizeof(ULONG);deliver(c);}
  }break;
 case IOCTL_BTHX_READ_HCI:
  if(!c->running||!c->negotiated){status=STATUS_DEVICE_NOT_READY;break;}
  status=WdfRequestRetrieveInputBuffer(request,sizeof(BTHX_HCI_PACKET_TYPE),&in,&length);if(!NT_SUCCESS(status))break;
  {unsigned type=*(BTHX_HCI_PACKET_TYPE *)in;
   if(type!=HciPacketEvent&&type!=HciPacketAclData){status=STATUS_INVALID_PARAMETER;break;}
   status=WdfRequestRetrieveOutputBuffer(request,BT_CONTEXT_BYTES+1,&out,&length);if(!NT_SUCCESS(status))break;
   status=WdfRequestForwardToIoQueue(request,c->reads[type==4?0:1]);
   if(NT_SUCCESS(status)){deliver(c);unlock(c);return;}
  }break;
 default:status=STATUS_NOT_SUPPORTED;break;
 }
 unlock(c);WdfRequestCompleteWithInformation(request,status,used);
}
NTSTATUS BtPrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 BT_DEVICE *c=BtContext(device);ULONG n,i;unsigned mem=0,irq=0;NTSTATUS status;
 if(c->referenced||c->uncertain||!raw||!translated||
    (n=WdfCmResourceListGetCount(raw))!=WdfCmResourceListGetCount(translated))return STATUS_DEVICE_CONFIGURATION_ERROR;
 for(i=0;i<n;i++){
  PCM_PARTIAL_RESOURCE_DESCRIPTOR r=WdfCmResourceListGetDescriptor(raw,i),t=WdfCmResourceListGetDescriptor(translated,i);
  if(!r||!t||r->Type!=t->Type||r->ShareDisposition!=CmResourceShareDeviceExclusive||t->ShareDisposition!=CmResourceShareDeviceExclusive)return STATUS_DEVICE_CONFIGURATION_ERROR;
  if(r->Type==CmResourceTypeMemory){
   if(mem++||r->u.Memory.Start.QuadPart!=0x11170000||r->u.Memory.Length!=0x1000||t->u.Memory.Length!=0x1000)return STATUS_DEVICE_CONFIGURATION_ERROR;
   c->mem=*r;c->trans_mem=*t;
  }else if(r->Type==CmResourceTypeInterrupt){
   if(irq++||r->u.Interrupt.Vector!=141||!t->u.Interrupt.Affinity||
      (r->Flags&CM_RESOURCE_INTERRUPT_LATCHED)||(t->Flags&CM_RESOURCE_INTERRUPT_LATCHED))return STATUS_DEVICE_CONFIGURATION_ERROR;
   c->irq_resource=*r;c->trans_irq=*t;
  }else return STATUS_DEVICE_CONFIGURATION_ERROR;
 }
 if(mem!=1||irq!=1)return STATUS_DEVICE_CONFIGURATION_ERROR;
 RtlZeroMemory(&c->parent,sizeof(c->parent));
 status=WdfFdoQueryForInterface(device,&A04SharedWifiInterfaceGuid,(PINTERFACE)&c->parent,sizeof(c->parent),A04_SHARED_WIFI_INTERFACE_VERSION,NULL);
 if(!NT_SUCCESS(status))return status;
 c->referenced=1;
 if(c->parent.Header.Size<sizeof(c->parent)||c->parent.Header.Version!=A04_SHARED_WIFI_INTERFACE_VERSION||
    !c->parent.Header.InterfaceDereference||!c->parent.Acquire||!c->parent.ReleaseQuiescent||
    !c->parent.Live||!c->parent.UpdateShared||!c->parent.WriteShared){c->uncertain=1;return STATUS_DEVICE_CONFIGURATION_ERROR;}
 c->removed=0;return STATUS_SUCCESS;
}
NTSTATUS BtStart(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
 BT_DEVICE *c=BtContext(device);struct a04_windows_fw_snapshot fw={0};UCHAR nvram[36]={0};NTSTATUS status=STATUS_DEVICE_NOT_READY;
 unsigned stage=0;const struct a04_wmt_startup_options options={1,1,0,0,2,0};
 UNREFERENCED_PARAMETER(previous);lock(c);
 if(!c->referenced||c->uncertain||c->leased||c->removed)goto done;
 /* Read all fixed and individual inputs before taking the physical lease. */
 stage=1;
 if(a04_windows_firmware_prepare_wmt(&fw,a04_wifi_wdf_wmt_sha256))goto done;
 stage=2;status=a04_windows_bt_nvram_read(nvram);if(!NT_SUCCESS(status))goto done;
 stage=3;status=c->parent.Acquire(c->parent.Header.Context,&c->generation);if(!NT_SUCCESS(status))goto done;
 c->leased=1;stage=4;status=STATUS_DEVICE_NOT_READY;
 if(!c->generation||c->parent.Live(c->parent.Header.Context,c->generation)!=1)goto fail;
 RtlZeroMemory(&c->owner,sizeof(c->owner));RtlZeroMemory(&c->life,sizeof(c->life));
 RtlZeroMemory(&c->board,sizeof(c->board));RtlZeroMemory(&c->bridge,sizeof(c->bridge));
 c->owner.generation=c->generation;c->owner.host=c->owner.gpio_clock=c->owner.combo=c->owner.firmware=c->owner.irq=1;
 if(a04_wifi_admit(&c->owner))goto fail;
 if(!NT_SUCCESS(a04_windows_msdc2_map_prepare(&c->host,&c->mem,&c->trans_mem,&c->owner,c->generation)))goto fail;
 WRITE_REGISTER_ULONG((volatile ULONG *)(c->host.base+0x10),0);
 if(READ_REGISTER_ULONG((volatile ULONG *)(c->host.base+0x10)))goto fail;
 if(!NT_SUCCESS(a04_windows_irq_connect(&c->irq,c->host.base,&c->irq_resource,&c->trans_irq,notify,c)))goto fail;
 if(!NT_SUCCESS(a04_windows_msdc2_activate(&c->host,&c->owner,c->generation,1)))goto fail;
 c->ops.context=c;c->ops.mask_and_drain_irq=mask;c->ops.drain_io=drain;c->ops.unmap_host=unmap;c->ops.release_lease=release;
 stage=5;
 if(a04_windows_wifi_board_init(&c->board_io,&c->parent,&c->host,&c->owner,c->generation)||
    a04_windows_wifi_control_open(&c->board_io,&c->board,&c->life,&c->bridge,0x00300000,&c->ops,&c->card))goto fail;
 stage=6;
 if(a04_wmt_bridge_boot_wmt(&c->bridge,fw.wmt,fw.wmt_length,a04_wifi_wdf_wmt_sha256,a04_sha256,&options))goto fail;
 stage=7;
 if(a04_bt_open(&c->bt,&c->bridge,c,receive)||a04_bt_initialize(&c->bt,nvram))goto fail;
 c->running=1;WdfIoQueueStart(c->reads[0]);WdfIoQueueStart(c->reads[1]);
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,
  "MT6630BT: ReadLocalVersion complete hci=%u revision=%u lmp=%u manufacturer=%u subversion=%u\n",
  c->bt.version.hci_version,c->bt.version.hci_revision,c->bt.version.lmp_version,c->bt.version.manufacturer,c->bt.version.lmp_subversion);
 WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(5));status=STATUS_SUCCESS;goto done;
fail:
 c->uncertain=1; /* no lease release on ambiguous initialization or failed PIO */
 if(c->irq.interrupt)(void)a04_windows_irq_remove(&c->irq);
 if(c->host.base)a04_windows_msdc2_unmap(&c->host);
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"MT6630BT: start failed status=%d; lease quarantined\n",c->bt.failed);
done:
 if(!NT_SUCCESS(status))DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,
  "MT6630BT: startup stage=%u status=0x%08lx\n",stage,(unsigned long)status);
 RtlSecureZeroMemory(nvram,sizeof(nvram));a04_windows_firmware_release(&fw);unlock(c);return status;
}
NTSTATUS BtStop(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
 BT_DEVICE *c=BtContext(device);NTSTATUS status=STATUS_SUCCESS;UNREFERENCED_PARAMETER(target);
 lock(c);c->running=0;unlock(c);WdfTimerStop(c->timer,TRUE);lock(c);c->negotiated=0;
 WdfIoQueuePurge(c->reads[0],NULL,NULL);WdfIoQueuePurge(c->reads[1],NULL,NULL);
 if(c->leased&&!c->uncertain&&!c->removed&&a04_wifi_life_stop(&c->life))c->uncertain=1;
 if(c->irq.interrupt&&a04_windows_irq_remove(&c->irq))c->uncertain=1;
 if(c->uncertain){if(c->host.base)a04_windows_msdc2_unmap(&c->host);status=STATUS_IO_DEVICE_ERROR;}
 RtlSecureZeroMemory(c->packets,sizeof(c->packets));c->packet_count=0;
 RtlSecureZeroMemory(c->bt.tx,sizeof(c->bt.tx));RtlSecureZeroMemory(c->bt.rx,sizeof(c->bt.rx));
 RtlSecureZeroMemory(c->bt.hci,sizeof(c->bt.hci));unlock(c);return status;
}
VOID BtRemoved(WDFDEVICE device)
{
 BT_DEVICE *c=BtContext(device);lock(c);c->running=0;unlock(c);WdfTimerStop(c->timer,TRUE);lock(c);
 c->removed=1;if(c->leased)c->uncertain=1;
 if(c->irq.interrupt)(void)a04_windows_irq_remove(&c->irq);
 /* No register access following surprise removal. */
 if(c->host.base)a04_windows_msdc2_unmap(&c->host);unlock(c);
}
NTSTATUS BtRelease(WDFDEVICE device,WDFCMRESLIST translated)
{
 BT_DEVICE *c=BtContext(device);UNREFERENCED_PARAMETER(translated);
 WdfTimerStop(c->timer,TRUE);lock(c);
 if(c->referenced&&!c->leased&&!c->uncertain){c->parent.Header.InterfaceDereference(c->parent.Header.Context);c->referenced=0;}
 unlock(c);return STATUS_SUCCESS;
}
static NTSTATUS child_create(WDFDEVICE device)
{
 BT_DEVICE *c=BtContext(device);PWDFDEVICE_INIT init=WdfPdoInitAllocate(device);UNICODE_STRING s;
 WDF_OBJECT_ATTRIBUTES attr;WDF_IO_QUEUE_CONFIG queue;NTSTATUS status;
 if(!init)return STATUS_INSUFFICIENT_RESOURCES;
 WdfDeviceInitSetDeviceType(init,FILE_DEVICE_BLUETOOTH);
 RtlInitUnicodeString(&s,L"A04BR3\\MT6630HCI");status=WdfPdoInitAssignDeviceID(init,&s);if(!NT_SUCCESS(status))goto fail;
 status=WdfPdoInitAddHardwareID(init,&s);if(!NT_SUCCESS(status))goto fail;
 RtlInitUnicodeString(&s,L"MS_BTHX_BTHMINI");status=WdfPdoInitAddCompatibleID(init,&s);if(!NT_SUCCESS(status))goto fail;
 RtlInitUnicodeString(&s,L"0");status=WdfPdoInitAssignInstanceID(init,&s);if(!NT_SUCCESS(status))goto fail;
 RtlInitUnicodeString(&s,L"MediaTek MT6630 Bluetooth (SDIO/STP)");
 {UNICODE_STRING location=RTL_CONSTANT_STRING(L"MSDC2 function 2 STP task 0");
  status=WdfPdoInitAddDeviceText(init,&s,&location,0x409);if(!NT_SUCCESS(status))goto fail;}
 WdfPdoInitSetDefaultLocale(init,0x409);
 WDF_OBJECT_ATTRIBUTES_INIT(&attr);attr.ExecutionLevel=WdfExecutionLevelPassive;
 status=WdfDeviceCreate(&init,&attr,&c->child);if(!NT_SUCCESS(status))goto fail;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchParallel);queue.EvtIoInternalDeviceControl=BtInternal;
 status=WdfIoQueueCreate(c->child,&queue,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);if(!NT_SUCCESS(status))goto delete_child;
 status=WdfFdoAddStaticChild(device,c->child);if(NT_SUCCESS(status))return status;
delete_child:WdfObjectDelete(c->child);c->child=NULL;return status;
fail:WdfDeviceInitFree(init);return status;
}
NTSTATUS BtAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDFDEVICE device;BT_DEVICE *c;WDF_OBJECT_ATTRIBUTES attr;WDF_PNPPOWER_EVENT_CALLBACKS pnp;
 WDF_TIMER_CONFIG timer;WDF_IO_QUEUE_CONFIG queue;NTSTATUS status;UNREFERENCED_PARAMETER(driver);
 WdfDeviceInitSetDeviceType(init,FILE_DEVICE_BUS_EXTENDER);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);pnp.EvtDevicePrepareHardware=BtPrepare;pnp.EvtDeviceReleaseHardware=BtRelease;
 pnp.EvtDeviceD0Entry=BtStart;pnp.EvtDeviceD0Exit=BtStop;pnp.EvtDeviceSurpriseRemoval=BtRemoved;
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,BT_DEVICE);attr.ExecutionLevel=WdfExecutionLevelPassive;
 status=WdfDeviceCreate(&init,&attr,&device);if(!NT_SUCCESS(status))return status;c=BtContext(device);c->device=device;
 WDF_OBJECT_ATTRIBUTES_INIT(&attr);attr.ParentObject=device;
 status=WdfWaitLockCreate(&attr,&c->lock);if(!NT_SUCCESS(status))return status;
 WDF_TIMER_CONFIG_INIT(&timer,BtTimer);timer.AutomaticSerialization=FALSE;
 attr.ExecutionLevel=WdfExecutionLevelPassive;status=WdfTimerCreate(&timer,&attr,&c->timer);if(!NT_SUCCESS(status))return status;
 for(unsigned i=0;i<2;i++){
  WDF_IO_QUEUE_CONFIG_INIT(&queue,WdfIoQueueDispatchManual);queue.PowerManaged=WdfFalse;
  status=WdfIoQueueCreate(device,&queue,WDF_NO_OBJECT_ATTRIBUTES,&c->reads[i]);if(!NT_SUCCESS(status))return status;
 }
 return child_create(device);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING path)
{WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,BtAdd);return WdfDriverCreate(object,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);}
