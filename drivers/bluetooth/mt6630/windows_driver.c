/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native SDIO/STP transport. No UART and no second MSDC2 owner. */
#include <ntddk.h>
#include <wdf.h>
#include <bthxddi.h>
#include "transport.h"
#include "status.h"
#include "hfp_target.h"
#include "hfp_controller_contract.h"
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
 struct a04_bt_hfp_parent hfp;UNICODE_STRING hfp_ready_link;int hfp_ready;LONG hfp_holds;int hfp_referenced,hfp_registered;
 A04_SHARED_WIFI_INTERFACE parent;ULONG generation;int referenced,leased,uncertain,removed,running,negotiated;
 CM_PARTIAL_RESOURCE_DESCRIPTOR mem,trans_mem,irq_resource,trans_irq;
 struct a04_wifi_owner owner;struct a04_windows_msdc2 host;struct a04_windows_irq irq;
 struct a04_windows_wifi_board board_io;struct a04_wifi_board board;struct a04_wifi_life life;
 struct a04_sdio_card_info card;struct a04_wifi_life_ops ops;struct a04_wmt_bridge bridge;
 MT6630_STATUS observation;
 struct a04_bt_transport bt;struct bt_packet packets[BT_SLOTS];unsigned packet_count;
} BT_DEVICE;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BT_DEVICE,BtContext)
EVT_WDF_DRIVER_DEVICE_ADD BtAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE BtPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE BtRelease;
EVT_WDF_DEVICE_D0_ENTRY BtStart;
EVT_WDF_DEVICE_D0_EXIT BtStop;
EVT_WDF_DEVICE_SURPRISE_REMOVAL BtRemoved;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL BtForward;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL BtInternal;
EVT_WDF_TIMER BtTimer;
EVT_WDF_OBJECT_CONTEXT_CLEANUP BtCleanup;
static void lock(BT_DEVICE *c){(void)WdfWaitLockAcquire(c->lock,NULL);}
static void unlock(BT_DEVICE *c){WdfWaitLockRelease(c->lock);}
static void controller_reference(PVOID p){BT_DEVICE *c=p;WdfObjectReference(c->device);}
static void controller_dereference(PVOID p){BT_DEVICE *c=p;WdfObjectDereference(c->device);}
static BOOLEAN controller_live(PVOID p,ULONG generation)
{
 BT_DEVICE *c=p;
 return generation&&generation==c->generation&&InterlockedCompareExchange(&c->hfp_holds,0,0)>0&&
  c->running&&c->leased&&!c->removed&&!c->uncertain&&c->parent.Live(c->parent.Header.Context,generation)==1;
}
static NTSTATUS controller_acquire(PVOID p,PULONG generation)
{
 BT_DEVICE *c=p;NTSTATUS status;
 if(!generation||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;*generation=0;
 status=WdfDeviceStopIdle(c->device,TRUE);if(!NT_SUCCESS(status))return status;
 lock(c);
 if(!c->running||!c->leased||c->removed||c->uncertain||!c->generation||
    c->hfp_holds==MAXLONG||!c->parent.Live(c->parent.Header.Context,c->generation))status=STATUS_DEVICE_NOT_READY;
 else {InterlockedIncrement(&c->hfp_holds);*generation=c->generation;status=STATUS_SUCCESS;}
 unlock(c);if(!NT_SUCCESS(status))WdfDeviceResumeIdle(c->device);return status;
}
static NTSTATUS controller_release(PVOID p,ULONG generation)
{
 BT_DEVICE *c=p;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(c);
 if(!generation||generation!=c->generation||c->hfp_holds<=0){unlock(c);return STATUS_INVALID_DEVICE_STATE;}
 InterlockedDecrement(&c->hfp_holds);unlock(c);WdfDeviceResumeIdle(c->device);return STATUS_SUCCESS;
}
static NTSTATUS controller_match(PVOID p,PDEVICE_OBJECT target)
{
 BT_DEVICE *c=p;ULONG generation;NTSTATUS status=controller_acquire(c,&generation),released;
 if(!NT_SUCCESS(status))return status;
 status=a04_bt_hfp_match_radio(WdfDeviceWdmGetDeviceObject(c->child),target);
 released=controller_release(c,generation);return NT_SUCCESS(released)?status:released;
}
/* Called under the BT lock after real WMT/FC72/ReadLocalVersion completion.
 * V2 parents may register real ownership without inventing PCM/latency proof.
 * Old V1 parents remain usable for raw HCI and receive no registration call. */
static NTSTATUS controller_register(BT_DEVICE *c)
{
 struct a04_bt_hfp_controller ops;NTSTATUS status;
 if(c->hfp_referenced||c->hfp_registered)return STATUS_INVALID_DEVICE_STATE;
 RtlZeroMemory(&c->hfp,sizeof(c->hfp));
 status=WdfFdoQueryForInterface(c->device,&A04BtSharedHfpGuid,(PINTERFACE)&c->hfp,
  sizeof(c->hfp),A04_BT_CONTROLLER_VERSION,NULL);
 if(!NT_SUCCESS(status))return status;
 c->hfp_referenced=1;
 if(c->hfp.Header.Size!=sizeof(c->hfp)||c->hfp.Header.Version!=A04_BT_CONTROLLER_VERSION||
    !c->hfp.Header.InterfaceDereference||!c->hfp.RegisterController||!c->hfp.UnregisterController){
  return STATUS_DEVICE_CONFIGURATION_ERROR;
 }
 RtlZeroMemory(&ops,sizeof(ops));ops.size=sizeof(ops);ops.version=A04_BT_CONTROLLER_VERSION;
 ops.context=c;ops.controller=WdfDeviceWdmGetDeviceObject(c->child);
 ops.Reference=controller_reference;ops.Dereference=controller_dereference;
 ops.Acquire=controller_acquire;ops.Live=controller_live;ops.Release=controller_release;ops.MatchHfpTarget=controller_match;
 /* Real FC72 decoding and measured codec latency are still missing. NULL
  * qualification fields are explicit absent capabilities, not callbacks. */
 status=c->hfp.RegisterController(c->hfp.Header.Context,&ops);
 if(NT_SUCCESS(status)){
  c->hfp_registered=1;
  status=IoSetDeviceInterfaceState(&c->hfp_ready_link,TRUE);
  if(NT_SUCCESS(status))c->hfp_ready=1;
 }
 return status;
}
static NTSTATUS controller_unregister(BT_DEVICE *c)
{
 NTSTATUS status;
 if(c->hfp_holds)return STATUS_DEVICE_BUSY;
 if(c->hfp_ready){status=IoSetDeviceInterfaceState(&c->hfp_ready_link,FALSE);
  if(!NT_SUCCESS(status))return status;c->hfp_ready=0;}
 if(c->hfp_registered){status=c->hfp.UnregisterController(c->hfp.Header.Context,c);
  if(!NT_SUCCESS(status))return status;c->hfp_registered=0;}
 if(c->hfp_referenced){
  if(!c->hfp.Header.InterfaceDereference)return STATUS_DEVICE_CONFIGURATION_ERROR;
  c->hfp.Header.InterfaceDereference(c->hfp.Header.Context);c->hfp_referenced=0;
 }
 RtlZeroMemory(&c->hfp,sizeof(c->hfp));return STATUS_SUCCESS;
}
/* Called with the FDO lock. Only allowlisted scalar fields enter the ABI. */
static void observe(BT_DEVICE *c,unsigned state)
{
 MT6630_STATUS *s=&c->observation;s->state=state;s->running=c->running;
 s->exclusive_bt_profile=1;s->transport_status=(ULONG)c->bt.failed;
 s->read_version_sent=c->bt.version_sent;s->read_version_complete=c->bt.version_complete;
 s->read_version_status=c->bt.version_complete?c->bt.version_status:MT6630_STATUS_UNKNOWN;
 s->read_version_length=c->bt.version_length;
 s->version_valid=c->bt.version_valid;
 s->hci_version=s->hci_revision=s->lmp_version=s->manufacturer=s->lmp_subversion=0;
 if(s->version_valid){s->hci_version=c->bt.version.hci_version;s->hci_revision=c->bt.version.hci_revision;
  s->lmp_version=c->bt.version.lmp_version;s->manufacturer=c->bt.version.manufacturer;s->lmp_subversion=c->bt.version.lmp_subversion;}
 a04_bt_status_publish(s);
}
static uint64_t bt_now_us(void *p){UNREFERENCED_PARAMETER(p);return KeQueryInterruptTime()/10u;}
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
   c->running=0;c->uncertain=1;observe(c,MT6630_STATE_TRANSPORT_FAILED);
   DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"MT6630BT: transport failed status=%d\n",c->bt.failed);
   WdfIoQueuePurge(c->reads[0],NULL,NULL);WdfIoQueuePurge(c->reads[1],NULL,NULL);
  }else {deliver(c);WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(5));}
 }
 unlock(c);
}
/* BthXDDI.h calls these kernel-level IOCTLs, but the documented major is
 * IRP_MJ_DEVICE_CONTROL. Only forward the five fixed BTHX operations from a
 * kernel caller. KMDF 1.9+ parent forwarding changes request ownership before
 * the FDO moves a READ to one of its own cancelable manual queues. */
VOID BtForward(WDFQUEUE queue,WDFREQUEST request,size_t outlen,size_t inlen,ULONG code)
{
 WDF_REQUEST_FORWARD_OPTIONS options;NTSTATUS status;
 UNREFERENCED_PARAMETER(outlen);UNREFERENCED_PARAMETER(inlen);
 if(WdfRequestGetRequestorMode(request)!=KernelMode){WdfRequestComplete(request,STATUS_ACCESS_DENIED);return;}
 switch(code){
 case IOCTL_BTHX_GET_VERSION:case IOCTL_BTHX_SET_VERSION:case IOCTL_BTHX_QUERY_CAPABILITIES:
 case IOCTL_BTHX_WRITE_HCI:case IOCTL_BTHX_READ_HCI:break;
 default:WdfRequestComplete(request,STATUS_NOT_SUPPORTED);return;
 }
 WDF_REQUEST_FORWARD_OPTIONS_INIT(&options);options.Flags=WDF_REQUEST_FORWARD_OPTION_SEND_AND_FORGET;
 status=WdfRequestForwardToParentDeviceIoQueue(request,
  WdfDeviceGetDefaultQueue(WdfPdoGetParent(WdfIoQueueGetDevice(queue))),&options);
 if(!NT_SUCCESS(status))WdfRequestComplete(request,status);
}
/* Both normal and internal kernel IOCTL majors are supported. The standard
 * stack uses the normal major; neither path accepts user HCI requests. */
VOID BtInternal(WDFQUEUE queue,WDFREQUEST request,size_t outlen,size_t inlen,ULONG code)
{
 BT_DEVICE *c=BtContext(WdfIoQueueGetDevice(queue));
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
  }
  /* The documented Windows 8/8.1 load contract rejects None/0. Preserve the
   * truthful reply, but record it as incompatible, never as stack success. */
  a04_bt_status_capabilities(NT_SUCCESS(status)?(PBTHX_CAPABILITIES)out:NULL,status);break;
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
static NTSTATUS prepare_impl(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
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
NTSTATUS BtPrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 NTSTATUS status=prepare_impl(device,raw,translated);BT_DEVICE *c=BtContext(device);
 if(!NT_SUCCESS(status)){lock(c);c->observation.stage=8;c->observation.start_status=(ULONG)status;
  observe(c,MT6630_STATE_PREPARE_FAILED);unlock(c);}
 return status;
}
NTSTATUS BtStart(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
 BT_DEVICE *c=BtContext(device);struct a04_windows_fw_snapshot fw={0};UCHAR nvram[36]={0};NTSTATUS status=STATUS_DEVICE_NOT_READY;
 /* a04br3_defconfig CONFIG_MTK_MERGE_INTERFACE_SUPPORT=y, and stock
  * mt6630_sw_init DAI_PAD after coex/before FM strap. This only initializes
  * chip pads via real WMT ACK; it is not proof of an active SCO audio route. */
 unsigned stage=0;const struct a04_wmt_startup_options options={1,1,0,1,2,0};
 UNREFERENCED_PARAMETER(previous);lock(c);
 if(!c->referenced||c->uncertain||c->leased||c->removed)goto done;
 c->observation.attempts++;c->observation.stage=0;c->observation.start_status=(ULONG)STATUS_PENDING;
 /* An early failure must not inherit a previous D0 attempt's version. */
 RtlZeroMemory(&c->bt,sizeof(c->bt));observe(c,MT6630_STATE_STARTING);
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
 if(a04_bt_open(&c->bt,&c->bridge,c,receive,bt_now_us)||a04_bt_initialize(&c->bt,nvram))goto fail;
 c->running=1;
 /* Registration is transport ownership only; it never qualifies HCIBypass.
  * Absence of the proposed V2 parent does not break the raw HCI profile. */
 status=controller_register(c);
 if(!NT_SUCCESS(status)){
  NTSTATUS cleanup=controller_unregister(c);
  if(!NT_SUCCESS(cleanup)){status=cleanup;goto fail;}
  if(status!=STATUS_NOT_SUPPORTED&&status!=STATUS_NOINTERFACE&&status!=STATUS_INVALID_PARAMETER&&status!=STATUS_REVISION_MISMATCH){goto fail;}
 }
 WdfIoQueueStart(c->reads[0]);WdfIoQueueStart(c->reads[1]);
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,
  "MT6630BT: ReadLocalVersion complete hci=%u revision=%u lmp=%u manufacturer=%u subversion=%u\n",
  c->bt.version.hci_version,c->bt.version.hci_revision,c->bt.version.lmp_version,c->bt.version.manufacturer,c->bt.version.lmp_subversion);
 WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(5));status=STATUS_SUCCESS;goto done;
fail:
 c->running=0;
 /* Preserve a concrete timeout rather than the last successful parent call. */
 if(c->bt.failed)status=c->bt.failed==-2?STATUS_IO_TIMEOUT:STATUS_IO_DEVICE_ERROR;
 else if(NT_SUCCESS(status)||status==STATUS_DEVICE_NOT_READY)status=STATUS_IO_DEVICE_ERROR;
 c->uncertain=1; /* no lease release on ambiguous initialization or failed PIO */
 if(c->irq.interrupt)(void)a04_windows_irq_remove(&c->irq);
 if(c->host.base)a04_windows_msdc2_unmap(&c->host);
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"MT6630BT: start failed status=%d; lease quarantined\n",c->bt.failed);
done:
 c->observation.stage=stage;c->observation.start_status=(ULONG)status;observe(c,MT6630_STATE_START_DONE);
 if(!NT_SUCCESS(status))DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,
  "MT6630BT: startup stage=%u status=0x%08lx\n",stage,(unsigned long)status);
 RtlSecureZeroMemory(nvram,sizeof(nvram));a04_windows_firmware_release(&fw);unlock(c);return status;
}
NTSTATUS BtStop(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
 BT_DEVICE *c=BtContext(device);NTSTATUS status=STATUS_SUCCESS;UNREFERENCED_PARAMETER(target);
 lock(c);
 status=controller_unregister(c);if(!NT_SUCCESS(status)){unlock(c);return status;}
 c->running=0;unlock(c);WdfTimerStop(c->timer,TRUE);lock(c);
 /* DDI version negotiation belongs to this PDO lifetime, not each D0 epoch. */
 WdfIoQueuePurge(c->reads[0],NULL,NULL);WdfIoQueuePurge(c->reads[1],NULL,NULL);
 if(c->leased&&!c->uncertain&&!c->removed&&a04_wifi_life_stop(&c->life))c->uncertain=1;
 if(c->irq.interrupt&&a04_windows_irq_remove(&c->irq))c->uncertain=1;
 if(c->uncertain){if(c->host.base)a04_windows_msdc2_unmap(&c->host);status=STATUS_IO_DEVICE_ERROR;}
 RtlSecureZeroMemory(c->packets,sizeof(c->packets));c->packet_count=0;
 RtlSecureZeroMemory(c->bt.tx,sizeof(c->bt.tx));RtlSecureZeroMemory(c->bt.rx,sizeof(c->bt.rx));
 RtlSecureZeroMemory(c->bt.hci,sizeof(c->bt.hci));observe(c,MT6630_STATE_STOPPED);unlock(c);return status;
}
VOID BtRemoved(WDFDEVICE device)
{
 BT_DEVICE *c=BtContext(device);lock(c);c->running=0;unlock(c);WdfTimerStop(c->timer,TRUE);lock(c);
 c->removed=1;if(c->leased)c->uncertain=1;
 /* Liveness is revoked first. Retain a busy registered owner until Audio
  * has drained; surprise removal is never proof that its DMA was stopped. */
 (void)controller_unregister(c);
 if(c->irq.interrupt)(void)a04_windows_irq_remove(&c->irq);
 /* No register access following surprise removal. */
 if(c->host.base)a04_windows_msdc2_unmap(&c->host);observe(c,MT6630_STATE_REMOVED);unlock(c);
}
NTSTATUS BtRelease(WDFDEVICE device,WDFCMRESLIST translated)
{
 BT_DEVICE *c=BtContext(device);UNREFERENCED_PARAMETER(translated);
 WdfTimerStop(c->timer,TRUE);lock(c);
 NTSTATUS hfp_status=controller_unregister(c);if(!NT_SUCCESS(hfp_status)){unlock(c);return hfp_status;}
 if(c->referenced&&!c->leased&&!c->uncertain){c->parent.Header.InterfaceDereference(c->parent.Header.Context);c->referenced=0;}
 unlock(c);return STATUS_SUCCESS;
}
static NTSTATUS child_create(WDFDEVICE device)
{
 BT_DEVICE *c=BtContext(device);PWDFDEVICE_INIT init=WdfPdoInitAllocate(device);UNICODE_STRING s;
 WDF_OBJECT_ATTRIBUTES attr;WDF_IO_QUEUE_CONFIG queue;NTSTATUS status;
 if(!init)return STATUS_INSUFFICIENT_RESOURCES;
 WdfDeviceInitSetDeviceType(init,FILE_DEVICE_BUS_EXTENDER);
 WdfPdoInitAllowForwardingRequestToParent(init);
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
 /* BthMini queries DDI version/capabilities before entering D0. Gating this
  * queue on D0 deadlocks its start. Data admission is checked by the FDO. */
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchParallel);queue.PowerManaged=WdfFalse;
 queue.EvtIoDeviceControl=BtForward;queue.EvtIoInternalDeviceControl=BtForward;
 status=WdfIoQueueCreate(c->child,&queue,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);if(!NT_SUCCESS(status))goto delete_child;
 status=WdfFdoAddStaticChild(device,c->child);if(NT_SUCCESS(status))return status;
delete_child:WdfObjectDelete(c->child);c->child=NULL;return status;
fail:WdfDeviceInitFree(init);return status;
}
VOID BtCleanup(WDFOBJECT object)
{
 BT_DEVICE *c=BtContext((WDFDEVICE)object);
 /* Registered owners/active references must have been removed by PnP Stop.
  * This callback never declares DMA quiescence or releases a combo lease. */
 if(c->hfp_ready_link.Buffer)RtlFreeUnicodeString(&c->hfp_ready_link);
}
NTSTATUS BtAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDFDEVICE device;BT_DEVICE *c;WDF_OBJECT_ATTRIBUTES attr;WDF_PNPPOWER_EVENT_CALLBACKS pnp;
 WDF_TIMER_CONFIG timer;WDF_IO_QUEUE_CONFIG queue;NTSTATUS status;UNREFERENCED_PARAMETER(driver);
 WdfDeviceInitSetDeviceType(init,FILE_DEVICE_BUS_EXTENDER);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);pnp.EvtDevicePrepareHardware=BtPrepare;pnp.EvtDeviceReleaseHardware=BtRelease;
 pnp.EvtDeviceD0Entry=BtStart;pnp.EvtDeviceD0Exit=BtStop;pnp.EvtDeviceSurpriseRemoval=BtRemoved;
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,BT_DEVICE);attr.ExecutionLevel=WdfExecutionLevelPassive;attr.EvtCleanupCallback=BtCleanup;
 status=WdfDeviceCreate(&init,&attr,&device);if(!NT_SUCCESS(status))return status;c=BtContext(device);c->device=device;
 /* Use manual IoRegister/IoSet state, not KMDF's automatic start-time enable:
  * the signal must follow actual parent registration, never device presence. */
 status=IoRegisterDeviceInterface(WdfDeviceWdmGetPhysicalDevice(device),&A04BtHfpControllerReadyGuid,NULL,&c->hfp_ready_link);
 if(!NT_SUCCESS(status))return status;
 WDF_OBJECT_ATTRIBUTES_INIT(&attr);attr.ParentObject=device;
 status=WdfWaitLockCreate(&attr,&c->lock);if(!NT_SUCCESS(status))return status;
 WDF_TIMER_CONFIG_INIT(&timer,BtTimer);timer.AutomaticSerialization=FALSE;
 attr.ExecutionLevel=WdfExecutionLevelPassive;status=WdfTimerCreate(&timer,&attr,&c->timer);if(!NT_SUCCESS(status))return status;
 for(unsigned i=0;i<2;i++){
  WDF_IO_QUEUE_CONFIG_INIT(&queue,WdfIoQueueDispatchManual);queue.PowerManaged=WdfFalse;
  status=WdfIoQueueCreate(device,&queue,WDF_NO_OBJECT_ATTRIBUTES,&c->reads[i]);if(!NT_SUCCESS(status))return status;
 }
 /* Match the child queue: metadata calls must reach the FDO before D0.
  * READ/WRITE explicitly require running + negotiated and do not power up HIF. */
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchParallel);queue.PowerManaged=WdfFalse;
 queue.EvtIoDeviceControl=BtInternal;queue.EvtIoInternalDeviceControl=BtInternal;
 status=WdfIoQueueCreate(device,&queue,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);if(!NT_SUCCESS(status))return status;
 return child_create(device);
}
EVT_WDF_DRIVER_UNLOAD BtUnload;
VOID BtUnload(WDFDRIVER driver){UNREFERENCED_PARAMETER(driver);a04_bt_status_delete();}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING path)
{
 WDFDRIVER driver;NTSTATUS status;WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,BtAdd);
 config.EvtDriverUnload=BtUnload;
 status=WdfDriverCreate(object,path,WDF_NO_OBJECT_ATTRIBUTES,&config,&driver);
 if(!NT_SUCCESS(status))return status;
 return a04_bt_status_create(driver);
}
