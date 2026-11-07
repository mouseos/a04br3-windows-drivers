/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Whole-combo cold-start Native 802.11 miniport. The NDIS work item is the
 * sole PASSIVE_LEVEL SDIO consumer (OID, polling RX and queued TX).
 * Device removal revokes admission before worker/interrupt rundown. */
#include "ndis_runtime.h"
#include "windows_calibration.h"
#include "wdf_hardware_config.h"
#include "arm_clock.h"
#include "windows_nvram_reader.h"
#define TAG 'r40A'
static NDIS_HANDLE driver_handle;
static uint64_t now(void *p);
static unsigned destination_kind(const UCHAR *p)
{unsigned all=255;for(unsigned i=0;i<6;i++)all&=p[i];return all==255?2:(p[0]&1?1:0);}
static void finish_tx(struct a04_ndis_runtime *a,PNET_BUFFER_LIST nbl,NDIS_STATUS status,ULONG flags)
{
 UCHAR snapshot[22];PVOID view=NULL;PNET_BUFFER nb=NET_BUFFER_LIST_FIRST_NB(nbl);ULONG bytes=nb?NET_BUFFER_DATA_LENGTH(nb):0;unsigned kind=0;
 if(nb&&bytes>=22)view=NdisGetDataBuffer(nb,22,snapshot,1,0);
 if(view)kind=destination_kind((const UCHAR *)view+16);
 NdisAcquireSpinLock(&a->lock);
 if(status==NDIS_STATUS_SUCCESS){a->stats.tx_packets[kind]++;a->stats.tx_bytes[kind]+=bytes;}
 else if(status==NDIS_STATUS_REQUEST_ABORTED||status==NDIS_STATUS_PAUSED||status==NDIS_STATUS_RESOURCES)a->stats.tx_discards++;
 else a->stats.tx_errors++;
 NdisReleaseSpinLock(&a->lock);
 NET_BUFFER_LIST_STATUS(nbl)=status;NdisMSendNetBufferListsComplete(a->adapter,nbl,flags);
}
void a04_ndis_tx_complete(void *context,PNET_BUFFER_LIST nbl,NDIS_STATUS status)
{finish_tx(context,nbl,status,0);}
static void api_leave(struct a04_ndis_runtime *a)
{NdisAcquireSpinLock(&a->lock);if(a->api_refs)--a->api_refs;if(!a->api_refs&&!a->busy)NdisSetEvent(&a->idle);NdisReleaseSpinLock(&a->lock);}
int a04_ndis_excluded(const struct a04_ndis_runtime *a,const UCHAR *mac)
{for(ULONG i=0;i<a->excluded_count;i++){unsigned d=0;for(unsigned j=0;j<6;j++)d|=mac[j]^a->excluded[i][j];if(!d)return 1;unsigned all=255;for(unsigned j=0;j<6;j++)all&=a->excluded[i][j];if(all==255)return 1;}return 0;}
static int exclude_bssid(void *context,const uint8_t *mac)
{return a04_ndis_excluded(context,mac);}

int a04_ndis_poll(struct a04_ndis_runtime *a)
{
 int result=a04_station_runtime_poll(&a->radio);
 /* A poll consumes at most64 actual HIF packets. All admitted RX frames
  * have independent nonpaged bytes/MDLs/NBLs until synchronous RESOURCES
  * indication returns. Pause/halt waits for this worker to finish them. */
 for(unsigned i=0;i<a->rx_count;i++){
  struct a04_rx_slot *slot=&a->rx[i];NDIS_STATUS status=NDIS_STATUS_PAUSED;
  if(!result&&!a->radio.failed&&!a->closing&&!a->paused&&a->power&&slot->epoch==a->receive_epoch){NET_BUFFER_DATA_LENGTH(NET_BUFFER_LIST_FIRST_NB(slot->nbl))=slot->length;status=a04_native_indicate_mpdu(a->adapter,slot->nbl,slot->bytes,slot->length,&slot->info);}
  unsigned kind=slot->length>=10?destination_kind(slot->bytes+4):0;
  NdisAcquireSpinLock(&a->lock);
  if(status==NDIS_STATUS_SUCCESS&&!slot->management){a->stats.rx_packets[kind]++;a->stats.rx_bytes[kind]+=slot->length;}else if(status!=NDIS_STATUS_SUCCESS)a->stats.rx_discards++;
  NdisReleaseSpinLock(&a->lock);RtlSecureZeroMemory(slot->bytes,sizeof(slot->bytes));slot->length=0;
 }
 a->rx_count=0;
 return result;
}
static uint64_t now(void *p){UNREFERENCED_PARAMETER(p);return a04_interrupt_time_us(KeQueryInterruptTime());}
static uint64_t system_time(void *p){LARGE_INTEGER t;UNREFERENCED_PARAMETER(p);KeQuerySystemTime(&t);return (uint64_t)t.QuadPart;}
static void wait_us(void *p,unsigned us){LARGE_INTEGER delay;UNREFERENCED_PARAMETER(p);delay.QuadPart=-(LONGLONG)us*10;(void)KeDelayExecutionThread(KernelMode,FALSE,&delay);}
static int cancelled(void *p){struct a04_ndis_runtime *a=p;return a->closing||a->cancelled;}
static void irq_notify(void *p){UNREFERENCED_PARAMETER(p);} /* deliberately polled; never arm SDIOIRQ */
static int connect_irq(void *p,struct a04_windows_msdc2 *host)
{
 struct a04_ndis_runtime *a=p;PDEVICE_OBJECT pdo;PCM_RESOURCE_LIST raw,trans;
 NdisMGetDeviceProperty(a->adapter,&pdo,NULL,NULL,&raw,&trans);UNREFERENCED_PARAMETER(pdo);
 if(!raw||!trans||raw->Count!=1||trans->Count!=1||raw->List[0].PartialResourceList.Count!=trans->List[0].PartialResourceList.Count)return -1;
 /* Exclusive cold-start host: no inherited command interrupt may race the
  * masked SDIO connection before board_start resets/configures the host. */
 WRITE_REGISTER_ULONG((volatile ULONG *)(host->base+0x10),0);
 if(READ_REGISTER_ULONG((volatile ULONG *)(host->base+0x10)))return -1;
 for(ULONG i=0;i<raw->List[0].PartialResourceList.Count;i++)if(raw->List[0].PartialResourceList.PartialDescriptors[i].Type==CmResourceTypeInterrupt)
  return NT_SUCCESS(a04_windows_irq_connect(&a->irq,host->base,&raw->List[0].PartialResourceList.PartialDescriptors[i],&trans->List[0].PartialResourceList.PartialDescriptors[i],irq_notify,a))?0:-1;
 return -1;
}
static int mask_irq(void *p){struct a04_ndis_runtime *a=p;return a->irq.interrupt?a04_windows_irq_stop(&a->irq):0;}
static int drain(void *p){UNREFERENCED_PARAMETER(p);return 0;} /* all synchronous I/O is this worker */
static int quiesce(void *p){struct a04_ndis_runtime *a=p;return a04_wifi_board_poweroff_signals(&a->session.diagnostic.board,&a->session.diagnostic.board_io.io);}
static NTSTATUS complete_query(PDEVICE_OBJECT d,PIRP irp,PVOID p)
{UNREFERENCED_PARAMETER(d);UNREFERENCED_PARAMETER(irp);KeSetEvent(p,IO_NO_INCREMENT,FALSE);return STATUS_MORE_PROCESSING_REQUIRED;}
static NTSTATUS query_parent(struct a04_ndis_runtime *a)
{
 PDEVICE_OBJECT pdo;PIRP irp;PIO_STACK_LOCATION sp;KEVENT done;NTSTATUS status;
 NdisMGetDeviceProperty(a->adapter,&pdo,NULL,NULL,NULL,NULL);if(!pdo)return STATUS_NO_SUCH_DEVICE;
 KeInitializeEvent(&done,NotificationEvent,FALSE);irp=IoAllocateIrp(pdo->StackSize,FALSE);if(!irp)return STATUS_INSUFFICIENT_RESOURCES;
 irp->IoStatus.Status=STATUS_NOT_SUPPORTED;sp=IoGetNextIrpStackLocation(irp);sp->MajorFunction=IRP_MJ_PNP;sp->MinorFunction=IRP_MN_QUERY_INTERFACE;
 sp->Parameters.QueryInterface.InterfaceType=&A04SharedWifiInterfaceGuid;sp->Parameters.QueryInterface.Size=sizeof(a->parent);
 sp->Parameters.QueryInterface.Version=A04_SHARED_WIFI_INTERFACE_VERSION;sp->Parameters.QueryInterface.Interface=(PINTERFACE)&a->parent;sp->Parameters.QueryInterface.InterfaceSpecificData=NULL;
 IoSetCompletionRoutine(irp,complete_query,&done,TRUE,TRUE,TRUE);status=IoCallDriver(pdo,irp);
 if(status==STATUS_PENDING)(void)KeWaitForSingleObject(&done,Executive,KernelMode,FALSE,NULL);
 status=irp->IoStatus.Status;IoFreeIrp(irp);if(!NT_SUCCESS(status))return status;
 a->referenced=1;
 if(a->parent.Header.Size!=sizeof(a->parent)||a->parent.Header.Version!=A04_SHARED_WIFI_INTERFACE_VERSION||!a->parent.Header.InterfaceDereference)return STATUS_DEVICE_CONFIGURATION_ERROR;
 return STATUS_SUCCESS;
}
static int firmware_event(void *p,const uint8_t *packet,unsigned bytes,uint64_t time)
{
 struct a04_ndis_runtime *a=p;
 /* Published gen3 EVENT_BSS_BEACON_TIMEOUT is four bytes. Lack of host
  * traffic alone is not proof of peer loss (firmware can filter beacons). */
 if(bytes>=8&&packet[4]==0x13){
  if(bytes!=12)return -1;
  if(a->radio.connected&&packet[8]==a->radio.station.local.bss&&packet[9]!=8&&
    time>=a->last_peer_rx&&time-a->last_peer_rx>=2000000u)
   a04_ndis_peer_disconnect(a,DOT11_ASSOC_STATUS_UNREACHABLE);
  return 0;
 }
 if(a04_os_security_event(a,packet,bytes))return -1;
 if(a->tx.pending){NDIS_STATUS s=a04_native_nbl_tx_event(&a->tx,&a->session.diagnostic.bridge,&a->radio.station,&a->security,packet,bytes,time);if(s!=NDIS_STATUS_SUCCESS&&s!=NDIS_STATUS_PENDING&&s!=NDIS_STATUS_NOT_SUPPORTED)return -1;}
 return 0;
}
int a04_ndis_rate_lookup(void *p,unsigned mode,unsigned code,ULONG *phy,UCHAR *rate)
{
 static const UCHAR cck[4]={2,4,11,22};static const UCHAR ofdm[8]={96,48,24,12,108,72,36,18};UNREFERENCED_PARAMETER(p);
 if(mode==0&&code<4)*rate=cck[code];else if(mode==1&&code>=8&&code<16)*rate=ofdm[code-8];else return -1;
 *phy=0;return 0;
}
static int receive_data(void *p,const uint8_t *packet,unsigned bytes)
{
 struct a04_ndis_runtime *a=p;struct a04_native_hif_rx rx;NDIS_STATUS status;struct a04_wlan_rx_frame management;
 if(!a04_wlan_rx_frame_parse(packet,bytes,&management)&&management.length>=24&&!(management.data[0]&0x0c)){
  if(management.length>=36&&(management.data[0]==0x80||management.data[0]==0x50)&&a04_wlan_equal(management.data+10,a->radio.station.ap.mac,6))a->last_peer_rx=now(a);
  if(management.length>=26&&(management.data[0]==0xc0||management.data[0]==0xa0)){
   if(!a04_wlan_disconnect_rx(&a->radio.station,&a->security,management.data,management.length))a04_ndis_peer_disconnect(a,(management.data[0]==0xc0?DOT11_ASSOC_STATUS_PEER_DEAUTHENTICATED:DOT11_ASSOC_STATUS_PEER_DISASSOCIATED)|a04_wmt_le16(management.data+24));
   return 0;
  }
  unsigned kind=destination_kind(management.data+4);ULONG flag=kind==2?NDIS_PACKET_TYPE_802_11_BROADCAST_MGMT:NDIS_PACKET_TYPE_802_11_DIRECTED_MGMT;
  if((kind==2||(kind==0&&a04_wlan_equal(management.data+4,a->radio.station.local.mac,6)))&&(a->filter&flag)&&!a->closing&&!a->paused&&a->rx_count<A04_NATIVE_QUEUE&&
    !a04_ndis_excluded(a,management.data+10)&&
    a04_native_hif_rx_management(packet,bytes,a->session.diagnostic.bridge.regulatory.permitted,a,a04_ndis_rate_lookup,&rx)==NDIS_STATUS_SUCCESS&&rx.length<=A04_NATIVE_MPDU_MAX){
   struct a04_rx_slot *slot=&a->rx[a->rx_count++];NdisMoveMemory(slot->bytes,rx.mpdu,rx.length);slot->length=rx.length;slot->info=rx.info;slot->management=TRUE;slot->epoch=a->receive_epoch;
  }
  /* Let the station owner consume beacons/probe responses even after STA
   * activation. Dropping an undecodable data frame must not swallow a
   * management frame needed by a subsequent real scan. */
  return A04_WLAN_UNSUPPORTED;
 }
 if(a->radio.station.secured){
  if(!a->association_reported){
   status=a04_native_hif_rx_decode_security(packet,bytes,a->radio.bridge->regulatory.permitted,a->radio.station.local.mac,a->radio.station.ap.mac,a,a04_ndis_rate_lookup,&rx);
   if(status==NDIS_STATUS_SUCCESS&&!a->early_length&&bytes<=sizeof(a->early_rx)){NdisMoveMemory(a->early_rx,packet,bytes);a->early_length=bytes;}
   return status==NDIS_STATUS_SUCCESS?0:A04_WLAN_UNSUPPORTED;
  }
  status=a04_os_security_receive(a,packet,bytes,&rx);
 }else status=a04_native_hif_rx_decode(packet,bytes,a->radio.bridge->regulatory.permitted,a->radio.station.local.mac,a->radio.station.ap.mac,a,a04_ndis_rate_lookup,&rx);
 if(status!=NDIS_STATUS_SUCCESS){NdisAcquireSpinLock(&a->lock);if(status==NDIS_STATUS_INVALID_DATA||status==NDIS_STATUS_INVALID_LENGTH)a->stats.rx_errors++;else a->stats.rx_discards++;NdisReleaseSpinLock(&a->lock);return 0;} /* never authorize a rejected frame */
 {unsigned group=rx.mpdu[4]&1,broadcast=1,eapol=rx.length>=32&&a04_wlan_be16(rx.mpdu+30)==0x888e;for(unsigned i=0;i<6;i++)if(rx.mpdu[4+i]!=255)broadcast=0;
  if(!eapol){if(!a->radio.station.secured&&a->os_security.exclude_unencrypted)return 0;if(!group&&!(a->filter&NDIS_PACKET_TYPE_DIRECTED))return 0;if(group&&broadcast&&!(a->filter&NDIS_PACKET_TYPE_BROADCAST))return 0;if(group&&!broadcast){
   int found=0;for(ULONG k=0;k<a->multicast_count;k++){unsigned d=0;for(unsigned j=0;j<6;j++)d|=rx.mpdu[4+j]^a->multicast[k][j];if(!d)found=1;}
   if(!(a->filter&NDIS_PACKET_TYPE_ALL_MULTICAST)&&(!(a->filter&NDIS_PACKET_TYPE_MULTICAST)||!found))return 0;
  }}}
 if(a->closing||a->paused||rx.length>A04_RUNTIME_MPDU_MAX||a->rx_count==A04_NATIVE_QUEUE){
  NdisAcquireSpinLock(&a->lock);a->stats.rx_discards++;if(a->rx_count==A04_NATIVE_QUEUE)a->stats.rx_no_buffer++;NdisReleaseSpinLock(&a->lock);return 0;
 }
 struct a04_rx_slot *slot=&a->rx[a->rx_count++];
 NdisMoveMemory(slot->bytes,rx.mpdu,rx.length);slot->length=rx.length;slot->info=rx.info;slot->management=FALSE;slot->epoch=a->receive_epoch;a->last_peer_rx=now(a);
 return 0;
}
void a04_ndis_association_reported(struct a04_ndis_runtime *a)
{
 a->association_reported=1;
 if(a->early_length)(void)receive_data(a,a->early_rx,a->early_length);
 RtlSecureZeroMemory(a->early_rx,sizeof(a->early_rx));a->early_length=0;
}
static int hardware_start(struct a04_ndis_runtime *a)
{
 PCM_RESOURCE_LIST raw,trans;CM_PARTIAL_RESOURCE_DESCRIPTOR *mem=NULL,*mapped=NULL;struct a04_windows_session_platform platform;
 NdisMGetDeviceProperty(a->adapter,NULL,NULL,NULL,&raw,&trans);
 if(!raw||!trans||raw->Count!=1||trans->Count!=1||raw->List[0].PartialResourceList.Count!=trans->List[0].PartialResourceList.Count)return -1;
 for(ULONG i=0;i<raw->List[0].PartialResourceList.Count;i++)if(raw->List[0].PartialResourceList.PartialDescriptors[i].Type==CmResourceTypeMemory){if(mem)return -1;mem=&raw->List[0].PartialResourceList.PartialDescriptors[i];mapped=&trans->List[0].PartialResourceList.PartialDescriptors[i];}
 if(!mem)return -1;
 platform.context=a;platform.connect_masked_irq=connect_irq;platform.mask_and_drain_irq=mask_irq;platform.drain_io=drain;platform.quiesce_combo=quiesce;
 /* Board facts: fixed3.3V, SDIO transport, vendor mt6630_ant_m1.cfg,
  * stp_uart_launcher default FM mode2; no shared26MHz/merge option. */
 {const struct a04_wmt_startup_options options={1,1,0,0,2,1};
 if(a04_windows_session_start(&a->session,&a->parent,&platform,mem,mapped,0x00300000,a04_wifi_wdf_wmt_sha256,&options))return -1;}
 a->radio.context=a;a->radio.now=now;a->radio.system_time=system_time;a->radio.wait=wait_us;a->radio.cancelled=cancelled;a->radio.event=firmware_event;a->radio.data=receive_data;a->radio.exclude_bssid=exclude_bssid;
 if(a04_wlan_native_be_enable(&a->session.diagnostic.bridge.wlan))return -1;
 if(a04_station_runtime_init(&a->radio,&a->session.diagnostic.bridge))return -1;
 return a04_ndis_power_profile(a,a->power_request,a->media_streaming,1);
}
/* gen3 CMD_ID_POWER_SAVE_MODE/CMD_PS_PROFILE_T, nicConfigPowerSaveProfile:
 * set, no response. Success means complete HIF submission, never a FW ACK.
 * Only the named Native levels with a direct gen3 counterpart are accepted. */
int a04_ndis_power_profile(struct a04_ndis_runtime *a,ULONG request,BOOLEAN streaming,int force)
{
 unsigned profile;
 switch(request){
 case DOT11_POWER_SAVING_NO_POWER_SAVING:profile=0;break;
 case DOT11_POWER_SAVING_FAST_PSP:profile=2;break;
 case DOT11_POWER_SAVING_MAX_PSP:profile=1;break;
 default:return -1;
 }
 if(streaming)profile=0;
 if((a->power||force)&&a->radio.initialized){
  uint8_t body[4]={(uint8_t)a->radio.station.local.bss,(uint8_t)profile,0,0};
  if(a04_wlan_set(&a->radio.bridge->wlan,5,body,sizeof(body),NULL))return -1;
 }
 a->power_request=request;a->media_streaming=streaming;return 0;
}
static int radio_setting(struct a04_ndis_runtime *a,int write)
{
 NDIS_STATUS status;NDIS_STRING key=RTL_CONSTANT_STRING(L"A04NicPowerState");
 if(write){NDIS_CONFIGURATION_PARAMETER value;NdisZeroMemory(&value,sizeof(value));value.ParameterType=NdisParameterInteger;value.ParameterData.IntegerData=!!a->power;
  NdisWriteConfiguration(&status,a->configuration,&key,&value);return status==NDIS_STATUS_SUCCESS?0:-1;}
 PNDIS_CONFIGURATION_PARAMETER value=NULL;NdisReadConfiguration(&status,&value,a->configuration,&key,NdisParameterInteger);
 if(status!=NDIS_STATUS_SUCCESS)return 0; /* MIB default is radio enabled. */
 if(!value||value->ParameterType!=NdisParameterInteger||value->ParameterData.IntegerData>1)return -1;
 a->power=(BOOLEAN)value->ParameterData.IntegerData;return 0;
}
static int disabled_identity(struct a04_ndis_runtime *a)
{
 /* Enumerate a retained radio-OFF adapter without powering the combo just
  * to obtain identity. The same checked NVRAM codec supplies MAC/domain. */
 UCHAR *raw=ExAllocatePoolWithTag(PagedPool,512,TAG);int result=-1;if(!raw)return -1;
 if(NT_SUCCESS(a04_windows_nvram_read(raw)))result=a04_regulatory_decode(raw,&a->session.diagnostic.bridge.regulatory);
 RtlSecureZeroMemory(raw,512);ExFreePoolWithTag(raw,TAG);a->radio.bridge=&a->session.diagnostic.bridge;return result;
}
int a04_ndis_radio_power(struct a04_ndis_runtime *a,int on)
{
 if(!!a->power==!!on)return radio_setting(a,1);
 if(!on){
  if(a04_windows_session_stop(&a->session)){a->uncertain=1;a->radio.failed=-1;return -1;}
  struct a04_os_security old=a->os_security;a04_os_security_clear(a);
  a->os_security.auth=old.auth;a->os_security.unicast=old.unicast;a->os_security.multicast=old.multicast;
  a->os_security.default_id=old.default_id;a->os_security.exclude_unencrypted=old.exclude_unencrypted;
  a->os_security.exemption_count=old.exemption_count;NdisMoveMemory(a->os_security.exemptions,old.exemptions,sizeof(old.exemptions));RtlSecureZeroMemory(&old,sizeof(old));
  a->radio.connected=a->radio.connecting=0;a->power=FALSE;return radio_setting(a,1);
 }
 RtlZeroMemory(&a->session,sizeof(a->session));RtlZeroMemory(&a->irq,sizeof(a->irq));RtlZeroMemory(&a->radio,sizeof(a->radio));
 RtlZeroMemory(&a->tx,sizeof(a->tx));a->tx.adapter=a->adapter;a->tx.completion_context=a;a->tx.completion=a04_ndis_tx_complete;
 if(hardware_start(a)){a->radio.failed=-1;return -1;}a->power=TRUE;return radio_setting(a,1);
}
int a04_ndis_radio_reset(struct a04_ndis_runtime *a)
{
 if(!a->power&&(a->session.state==A04_SESSION_OFF||a->session.state==A04_SESSION_STOPPED)){
  a04_os_security_clear(a);return 0; /* quiescent OFF epoch; do not turn RF on */
 }
 if(a04_windows_session_stop(&a->session)){a->uncertain=1;a->radio.failed=-1;return -1;}
 RtlZeroMemory(&a->session,sizeof(a->session));RtlZeroMemory(&a->irq,sizeof(a->irq));
 RtlZeroMemory(&a->radio,sizeof(a->radio));
 {struct a04_os_security old=a->os_security;a04_os_security_clear(a);a->os_security.auth=old.auth;a->os_security.unicast=old.unicast;a->os_security.multicast=old.multicast;a->os_security.default_id=old.default_id;a->os_security.exclude_unencrypted=old.exclude_unencrypted;a->os_security.exemption_count=old.exemption_count;NdisMoveMemory(a->os_security.exemptions,old.exemptions,sizeof(old.exemptions));RtlSecureZeroMemory(&old,sizeof(old));}
 a->association_reported=a->early_length=0;RtlSecureZeroMemory(a->early_rx,sizeof(a->early_rx));
 RtlZeroMemory(&a->tx,sizeof(a->tx));a->tx.adapter=a->adapter;a->tx.completion_context=a;a->tx.completion=a04_ndis_tx_complete;
 if(hardware_start(a)){a->radio.failed=-1;return -1;}return 0;
}
static void worker(PVOID p,NDIS_HANDLE work);
static int queue_work(struct a04_ndis_runtime *a)
{
 NDIS_HANDLE handle=NdisAllocateIoWorkItem(a->adapter);if(!handle)return -1;
 NdisResetEvent(&a->idle);a->busy=1;NdisQueueIoWorkItem(handle,worker,a);return 0;
}
static void worker(PVOID p,NDIS_HANDLE work)
{
 struct a04_ndis_runtime *a=p;PNDIS_OID_REQUEST request;PNET_BUFFER_LIST send;NDIS_STATUS s;

again:
 NdisAcquireSpinLock(&a->lock);request=a->request;
 if(!request&&!a->send)a->send=a04_queue_take(&a->send_queue);
 send=a->send;NdisReleaseSpinLock(&a->lock);
 if(request){s=a->cancelled||a->closing?NDIS_STATUS_REQUEST_ABORTED:a04_ndis_control(a,request);NdisAcquireSpinLock(&a->lock);a->request=NULL;NdisReleaseSpinLock(&a->lock);if(s!=NDIS_STATUS_PENDING)NdisMOidRequestComplete(a->adapter,request,s);}
 else if(send){
  s=a->closing?NDIS_STATUS_REQUEST_ABORTED:(a->paused?NDIS_STATUS_PAUSED:NDIS_STATUS_ADAPTER_NOT_READY);
  if(!a->closing&&!a->paused&&a->power&&a->radio.connected){uint64_t end=now(a)+900000;
   s=a->radio.station.secured?a04_os_security_send(a,send,now(a),end):a04_native_nbl_send(&a->tx,&a->session.diagnostic.bridge,&a->radio.station,&a->security,send,a->mpdu,sizeof(a->mpdu),a->ethernet,sizeof(a->ethernet),NULL,NULL,now(a),end);
   if(s==NDIS_STATUS_PENDING){while(a->tx.pending&&!a->closing&&now(a)<end){if(a04_ndis_poll(a))break;wait_us(a,1000);}if(a->tx.pending)(void)a04_native_nbl_abort(&a->tx,&a->session.diagnostic.bridge);}
  }
  RtlSecureZeroMemory(a->mpdu,sizeof(a->mpdu));RtlSecureZeroMemory(a->ethernet,sizeof(a->ethernet));RtlSecureZeroMemory(a->session.diagnostic.bridge.wlan.tx,sizeof(a->session.diagnostic.bridge.wlan.tx));
  if(s!=NDIS_STATUS_PENDING)a04_ndis_tx_complete(a,send,s);
  NdisAcquireSpinLock(&a->lock);a->send=NULL;a04_queue_release(&a->send_queue);NdisReleaseSpinLock(&a->lock);
 }else if(!a->closing&&!a->paused&&a->power)(void)a04_ndis_poll(a);
 NdisAcquireSpinLock(&a->lock);
 if(a->next_request){a->request=a->next_request;a->next_request=NULL;a->cancelled=a->next_cancelled;a->next_cancelled=0;NdisReleaseSpinLock(&a->lock);goto again;}
 if(a->send_queue.count){NdisReleaseSpinLock(&a->lock);goto again;}
 NdisFreeIoWorkItem(work);a->busy=0;a->cancelled=0;if(!a->api_refs)NdisSetEvent(&a->idle);NdisReleaseSpinLock(&a->lock);
}
static void poll_dpc(PKDPC dpc,PVOID context,PVOID x,PVOID y)
{
 struct a04_ndis_runtime *a=context;UNREFERENCED_PARAMETER(dpc);UNREFERENCED_PARAMETER(x);UNREFERENCED_PARAMETER(y);
 NdisAcquireSpinLock(&a->lock);if(!a->closing&&!a->paused&&!a->busy){(void)queue_work(a);}NdisReleaseSpinLock(&a->lock);
}
static void close_worker(struct a04_ndis_runtime *a)
{
 NdisAcquireSpinLock(&a->lock);a->closing=1;NdisReleaseSpinLock(&a->lock);
 (void)KeCancelTimer(&a->timer);KeFlushQueuedDpcs();while(!NdisWaitEvent(&a->idle,1000)){}
 NdisAcquireSpinLock(&a->lock);NdisReleaseSpinLock(&a->lock);
}
static void cleanup(struct a04_ndis_runtime *a)
{
 if(a->configuration){NdisCloseConfiguration(a->configuration);a->configuration=NULL;}
 a04_os_security_clear(a);RtlSecureZeroMemory(a->early_rx,sizeof(a->early_rx));RtlSecureZeroMemory(a->mpdu,sizeof(a->mpdu));RtlSecureZeroMemory(a->ethernet,sizeof(a->ethernet));RtlSecureZeroMemory(a->rx_bytes,sizeof(a->rx_bytes));
 if(a->session.state==A04_SESSION_READY||a->session.state==A04_SESSION_QUARANTINED){if(a04_windows_session_stop(&a->session))a->uncertain=1;}
 if(a->irq.interrupt)(void)a04_windows_irq_remove(&a->irq);
 /* Do not free objects whose live MMIO/lease cannot be released. */
 if(a->uncertain||a->session.state==A04_SESSION_QUARANTINED)return;
 if(a->referenced&&a->parent.Header.InterfaceDereference)a->parent.Header.InterfaceDereference(a->parent.Header.Context);
 for(unsigned i=0;i<A04_NATIVE_QUEUE;i++){if(a->rx[i].nbl)NdisFreeNetBufferList(a->rx[i].nbl);if(a->rx[i].mdl)IoFreeMdl(a->rx[i].mdl);}if(a->rx_pool)NdisFreeNetBufferListPool(a->rx_pool);
NdisFreeSpinLock(&a->lock);RtlSecureZeroMemory(a,sizeof(*a));NdisFreeMemory(a,0,0);
}
MINIPORT_INITIALIZE RuntimeInitialize;MINIPORT_HALT RuntimeHalt;MINIPORT_PAUSE RuntimePause;MINIPORT_RESTART RuntimeRestart;
MINIPORT_OID_REQUEST RuntimeOid;MINIPORT_CANCEL_OID_REQUEST RuntimeCancelOid;MINIPORT_SEND_NET_BUFFER_LISTS RuntimeSend;
MINIPORT_RETURN_NET_BUFFER_LISTS RuntimeReturn;MINIPORT_CANCEL_SEND RuntimeCancelSend;MINIPORT_SHUTDOWN RuntimeShutdown;
MINIPORT_DIRECT_OID_REQUEST RuntimeDirectOid;
MINIPORT_CANCEL_DIRECT_OID_REQUEST RuntimeCancelDirectOid;
DRIVER_UNLOAD RuntimeUnload;
NDIS_STATUS RuntimeInitialize(NDIS_HANDLE adapter,NDIS_HANDLE context,PNDIS_MINIPORT_INIT_PARAMETERS params)
{
 struct a04_ndis_runtime *a;NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES attr;NET_BUFFER_LIST_POOL_PARAMETERS pool;NDIS_STATUS s;LARGE_INTEGER due;
 UNREFERENCED_PARAMETER(context);UNREFERENCED_PARAMETER(params);
 a=NdisAllocateMemoryWithTagPriority(adapter,sizeof(*a),TAG,NormalPoolPriority);if(!a)return NDIS_STATUS_RESOURCES;NdisZeroMemory(a,sizeof(*a));a->adapter=adapter;a->paused=1;a->power=TRUE;a->lookahead=A04_RUNTIME_MTU;
 NdisAllocateSpinLock(&a->lock);NdisInitializeEvent(&a->idle);NdisSetEvent(&a->idle);KeInitializeTimer(&a->timer);KeInitializeDpc(&a->dpc,poll_dpc,a);
 NdisZeroMemory(&attr,sizeof(attr));attr.Header.Type=NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;attr.Header.Revision=NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_2;attr.Header.Size=NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_2;attr.MiniportAdapterContext=a;attr.InterfaceType=NdisInterfaceInternal;
 s=NdisMSetMiniportAttributes(adapter,(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&attr);if(s!=NDIS_STATUS_SUCCESS)goto failed;
 NdisZeroMemory(&pool,sizeof(pool));pool.Header.Type=NDIS_OBJECT_TYPE_DEFAULT;pool.Header.Revision=1;pool.Header.Size=sizeof(pool);pool.ProtocolId=NDIS_PROTOCOL_ID_DEFAULT;pool.fAllocateNetBuffer=TRUE;pool.PoolTag=TAG;
 a->rx_pool=NdisAllocateNetBufferListPool(adapter,&pool);
 if(!a->rx_pool){s=NDIS_STATUS_RESOURCES;goto failed;}
 for(unsigned i=0;i<A04_NATIVE_QUEUE;i++){
  struct a04_rx_slot *slot=&a->rx[i];slot->mdl=IoAllocateMdl(slot->bytes,sizeof(slot->bytes),FALSE,FALSE,NULL);
  if(!slot->mdl){s=NDIS_STATUS_RESOURCES;goto failed;}MmBuildMdlForNonPagedPool(slot->mdl);
  slot->nbl=NdisAllocateNetBufferAndNetBufferList(a->rx_pool,0,0,slot->mdl,0,sizeof(slot->bytes));
  if(!slot->nbl){s=NDIS_STATUS_RESOURCES;goto failed;}
 }
 NDIS_CONFIGURATION_OBJECT configuration;NdisZeroMemory(&configuration,sizeof(configuration));
 configuration.Header.Type=NDIS_OBJECT_TYPE_CONFIGURATION_OBJECT;configuration.Header.Revision=NDIS_CONFIGURATION_OBJECT_REVISION_1;configuration.Header.Size=NDIS_SIZEOF_CONFIGURATION_OBJECT_REVISION_1;configuration.NdisHandle=adapter;
 s=NdisOpenConfigurationEx(&configuration,&a->configuration);if(s!=NDIS_STATUS_SUCCESS)goto failed;
 if(radio_setting(a,0)||!NT_SUCCESS(query_parent(a))||(a->power?hardware_start(a):disabled_identity(a))){s=NDIS_STATUS_ADAPTER_NOT_READY;goto failed;}
 a->tx.adapter=adapter;a->tx.completion_context=a;a->tx.completion=a04_ndis_tx_complete;a->os_security.auth=DOT11_AUTH_ALGO_80211_OPEN;s=a04_ndis_attributes(a);if(s!=NDIS_STATUS_SUCCESS)goto failed;
 due.QuadPart=-100000;KeSetTimerEx(&a->timer,due,10,&a->dpc);return NDIS_STATUS_SUCCESS;
failed:cleanup(a);return s;
}
VOID RuntimeHalt(NDIS_HANDLE p,NDIS_HALT_ACTION action){struct a04_ndis_runtime *a=p;UNREFERENCED_PARAMETER(action);close_worker(a);cleanup(a);}
NDIS_STATUS RuntimePause(NDIS_HANDLE p,PNDIS_MINIPORT_PAUSE_PARAMETERS params)
{struct a04_ndis_runtime *a=p;UNREFERENCED_PARAMETER(params);NdisAcquireSpinLock(&a->lock);a->paused=1;NdisReleaseSpinLock(&a->lock);while(!NdisWaitEvent(&a->idle,1000)){}NdisAcquireSpinLock(&a->lock);NdisReleaseSpinLock(&a->lock);return NDIS_STATUS_SUCCESS;}
NDIS_STATUS RuntimeRestart(NDIS_HANDLE p,PNDIS_MINIPORT_RESTART_PARAMETERS params)
{struct a04_ndis_runtime *a=p;UNREFERENCED_PARAMETER(params);if(a->radio.failed||a->closing)return NDIS_STATUS_ADAPTER_NOT_READY;NdisAcquireSpinLock(&a->lock);a->paused=0;NdisReleaseSpinLock(&a->lock);return NDIS_STATUS_SUCCESS;}
NDIS_STATUS RuntimeOid(NDIS_HANDLE p,PNDIS_OID_REQUEST request)
{
 struct a04_ndis_runtime *a=p;NDIS_STATUS s;NdisAcquireSpinLock(&a->lock);
 if(a->closing)s=NDIS_STATUS_ADAPTER_NOT_READY;else if(a->busy){if(a->next_request)s=NDIS_STATUS_DOT11_MEDIA_IN_USE;else{a->next_request=request;a->next_cancelled=0;s=NDIS_STATUS_PENDING;}}
 else{a->request=request;a->cancelled=0;if(queue_work(a)){a->request=NULL;s=NDIS_STATUS_RESOURCES;}else s=NDIS_STATUS_PENDING;}
 NdisReleaseSpinLock(&a->lock);return s;
}
/* NDIS 6.20+ requires both direct-OID entry points, even without direct
 * offloads. Direct requests are not serialized with RuntimeOid and must not
 * enter its single PASSIVE_LEVEL hardware queue. No direct OID is advertised
 * or retained: reject synchronously, with no completion/cancellation state. */
_Use_decl_annotations_
NDIS_STATUS RuntimeDirectOid(NDIS_HANDLE p,PNDIS_OID_REQUEST request)
{
 UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(request);
 return NDIS_STATUS_NOT_SUPPORTED;
}
_Use_decl_annotations_
VOID RuntimeCancelDirectOid(NDIS_HANDLE p,PVOID id)
{UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(id);}
VOID RuntimeCancelOid(NDIS_HANDLE p,PVOID id){struct a04_ndis_runtime *a=p;NdisAcquireSpinLock(&a->lock);if(a->request&&a->request->RequestId==id)a->cancelled=1;if(a->next_request&&a->next_request->RequestId==id)a->next_cancelled=1;NdisReleaseSpinLock(&a->lock);}
VOID RuntimeSend(NDIS_HANDLE p,PNET_BUFFER_LIST list,NDIS_PORT_NUMBER port,ULONG flags)
{
 struct a04_ndis_runtime *a=p;ULONG completion=(flags&NDIS_SEND_FLAGS_DISPATCH_LEVEL)?NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL:0;
 NdisAcquireSpinLock(&a->lock);a->api_refs++;NdisResetEvent(&a->idle);NdisReleaseSpinLock(&a->lock);
 while(list){
  PNET_BUFFER_LIST n=list;NDIS_STATUS status=NDIS_STATUS_PENDING;list=NET_BUFFER_LIST_NEXT_NBL(n);NET_BUFFER_LIST_NEXT_NBL(n)=NULL;
  NdisAcquireSpinLock(&a->lock);
  if(a->closing)status=NDIS_STATUS_REQUEST_ABORTED;else if(a->paused)status=NDIS_STATUS_PAUSED;
  else if(port||!NET_BUFFER_LIST_FIRST_NB(n)||NET_BUFFER_NEXT_NB(NET_BUFFER_LIST_FIRST_NB(n)))status=NDIS_STATUS_NOT_SUPPORTED;
  else if(NET_BUFFER_DATA_LENGTH(NET_BUFFER_LIST_FIRST_NB(n))>A04_RUNTIME_MPDU_MAX)status=NDIS_STATUS_INVALID_LENGTH;
  else if(a04_queue_put(&a->send_queue,n))status=NDIS_STATUS_RESOURCES;
  else if(!a->busy&&queue_work(a)){(void)a04_queue_remove(&a->send_queue,a->send_queue.count-1);status=NDIS_STATUS_RESOURCES;}
  NdisReleaseSpinLock(&a->lock);
  if(status!=NDIS_STATUS_PENDING)finish_tx(a,n,status,completion);
 }
 api_leave(a);
}
VOID RuntimeReturn(NDIS_HANDLE p,PNET_BUFFER_LIST n,ULONG flags){UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(n);UNREFERENCED_PARAMETER(flags);} /* RESOURCES only: NDIS never retains the64 pool entries. */
VOID RuntimeCancelSend(NDIS_HANDLE p,PVOID id)
{
 struct a04_ndis_runtime *a=p;PNET_BUFFER_LIST cancelled[A04_NATIVE_QUEUE];unsigned count=0;
 NdisAcquireSpinLock(&a->lock);a->api_refs++;NdisResetEvent(&a->idle);
 for(unsigned i=0;i<a->send_queue.count;){PNET_BUFFER_LIST n=a->send_queue.items[(a->send_queue.head+i)%A04_NATIVE_QUEUE];if(NDIS_GET_NET_BUFFER_LIST_CANCEL_ID(n)==id)cancelled[count++]=a04_queue_remove(&a->send_queue,i);else i++;}
 NdisReleaseSpinLock(&a->lock);
 for(unsigned i=0;i<count;i++)finish_tx(a,cancelled[i],NDIS_STATUS_REQUEST_ABORTED,KeGetCurrentIrql()==DISPATCH_LEVEL?NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL:0);
 /* In-flight SDIO may already be on air. Drain its real TX_DONE, never
  * fabricate abort/completion or release its NBL twice. */
 api_leave(a);
}
VOID RuntimeShutdown(NDIS_HANDLE p,NDIS_SHUTDOWN_ACTION action){struct a04_ndis_runtime *a=p;UNREFERENCED_PARAMETER(action);InterlockedExchange(&a->closing,1);}
MINIPORT_DEVICE_PNP_EVENT_NOTIFY RuntimePnp;
VOID RuntimePnp(NDIS_HANDLE p,PNET_DEVICE_PNP_EVENT event)
{
 struct a04_ndis_runtime *a=p;
 if(event->DevicePnPEvent==NdisDevicePnPEventSurpriseRemoved){
  close_worker(a);(void)a04_windows_irq_remove(&a->irq);
  a->uncertain=1;a->radio.failed=-1;
 }
}
VOID RuntimeUnload(PDRIVER_OBJECT p){UNREFERENCED_PARAMETER(p);if(driver_handle)NdisMDeregisterMiniportDriver(driver_handle);}
NTSTATUS DriverEntry(PDRIVER_OBJECT p,PUNICODE_STRING path)
{
 NDIS_MINIPORT_DRIVER_CHARACTERISTICS c;NdisZeroMemory(&c,sizeof(c));c.Header.Type=NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;c.Header.Revision=NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;c.Header.Size=NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;c.MajorNdisVersion=6;c.MinorNdisVersion=30;c.MajorDriverVersion=0;c.MinorDriverVersion=2;
 c.InitializeHandlerEx=RuntimeInitialize;c.HaltHandlerEx=RuntimeHalt;c.PauseHandler=RuntimePause;c.RestartHandler=RuntimeRestart;c.OidRequestHandler=RuntimeOid;c.CancelOidRequestHandler=RuntimeCancelOid;c.SendNetBufferListsHandler=RuntimeSend;c.ReturnNetBufferListsHandler=RuntimeReturn;c.CancelSendHandler=RuntimeCancelSend;c.ShutdownHandlerEx=RuntimeShutdown;c.UnloadHandler=RuntimeUnload;c.DevicePnPEventNotifyHandler=RuntimePnp;
 c.DirectOidRequestHandler=RuntimeDirectOid;c.CancelDirectOidRequestHandler=RuntimeCancelDirectOid;
 return NdisMRegisterMiniportDriver(p,path,NULL,&c,&driver_handle);
}
