/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Whole-combo cold-start Native 802.11 miniport. The NDIS work item is the
 * sole PASSIVE_LEVEL SDIO consumer (OID, polling RX and queued TX).
 * Device removal revokes admission before worker/interrupt rundown. */
#include "ndis_runtime.h"
#include "windows_calibration.h"
#include "wdf_hardware_config.h"
#include "arm_clock.h"
#define TAG 'r40A'
static NDIS_HANDLE driver_handle;
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
  if(management.length>=26&&(management.data[0]==0xc0||management.data[0]==0xa0)){
   if(!a04_wlan_disconnect_rx(&a->radio.station,&a->security,management.data,management.length))a04_ndis_peer_disconnect(a,(management.data[0]==0xc0?DOT11_ASSOC_STATUS_PEER_DEAUTHENTICATED:DOT11_ASSOC_STATUS_PEER_DISASSOCIATED)|a04_wmt_le16(management.data+24));
   return 0;
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
 if(status!=NDIS_STATUS_SUCCESS)return 0; /* reject unauthenticated/malformed packets without authorizing */
 {unsigned group=rx.mpdu[4]&1,broadcast=1,eapol=rx.length>=32&&a04_wlan_be16(rx.mpdu+30)==0x888e;for(unsigned i=0;i<6;i++)if(rx.mpdu[4+i]!=255)broadcast=0;
  if(!eapol){if(!a->radio.station.secured&&a->os_security.exclude_unencrypted)return 0;if(!group&&!(a->filter&NDIS_PACKET_TYPE_DIRECTED))return 0;if(group&&broadcast&&!(a->filter&NDIS_PACKET_TYPE_BROADCAST))return 0;if(group&&!broadcast&&!(a->filter&NDIS_PACKET_TYPE_MULTICAST))return 0;}}
 if(rx.length>sizeof(a->rx_bytes))return 0;
 NET_BUFFER_DATA_LENGTH(NET_BUFFER_LIST_FIRST_NB(a->rx_nbl))=rx.length;
 status=a04_native_indicate_mpdu(a->adapter,a->rx_nbl,rx.mpdu,rx.length,&rx.info);
 RtlSecureZeroMemory(a->rx_bytes,sizeof(a->rx_bytes));return status==NDIS_STATUS_SUCCESS?0:-1;
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
 a->radio.context=a;a->radio.now=now;a->radio.system_time=system_time;a->radio.wait=wait_us;a->radio.cancelled=cancelled;a->radio.event=firmware_event;a->radio.data=receive_data;
 return a04_station_runtime_init(&a->radio,&a->session.diagnostic.bridge);
}
int a04_ndis_radio_reset(struct a04_ndis_runtime *a)
{
 if(a04_windows_session_stop(&a->session)){a->uncertain=1;a->radio.failed=-1;return -1;}
 RtlZeroMemory(&a->session,sizeof(a->session));RtlZeroMemory(&a->irq,sizeof(a->irq));
 RtlZeroMemory(&a->radio,sizeof(a->radio));
 {struct a04_os_security old=a->os_security;a04_os_security_clear(a);a->os_security.auth=old.auth;a->os_security.unicast=old.unicast;a->os_security.multicast=old.multicast;a->os_security.default_id=old.default_id;a->os_security.exclude_unencrypted=old.exclude_unencrypted;a->os_security.exemption_count=old.exemption_count;NdisMoveMemory(a->os_security.exemptions,old.exemptions,sizeof(old.exemptions));RtlSecureZeroMemory(&old,sizeof(old));}
 a->association_reported=a->early_length=0;RtlSecureZeroMemory(a->early_rx,sizeof(a->early_rx));
 RtlZeroMemory(&a->tx,sizeof(a->tx));a->tx.adapter=a->adapter;
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
 NdisAcquireSpinLock(&a->lock);request=a->request;send=a->send;NdisReleaseSpinLock(&a->lock);
 if(request){s=a->cancelled||a->closing?NDIS_STATUS_REQUEST_ABORTED:a04_ndis_control(a,request);NdisAcquireSpinLock(&a->lock);a->request=NULL;NdisReleaseSpinLock(&a->lock);if(s!=NDIS_STATUS_PENDING)NdisMOidRequestComplete(a->adapter,request,s);}
 else if(send){
  s=NDIS_STATUS_ADAPTER_NOT_READY;
  if(!a->closing&&!a->paused&&a->radio.connected){uint64_t end=now(a)+900000;
   s=a->radio.station.secured?a04_os_security_send(a,send,now(a),end):a04_native_nbl_send(&a->tx,&a->session.diagnostic.bridge,&a->radio.station,&a->security,send,a->mpdu,sizeof(a->mpdu),a->ethernet,sizeof(a->ethernet),NULL,NULL,now(a),end);
   if(s==NDIS_STATUS_PENDING){while(a->tx.pending&&!a->closing&&now(a)<end){if(a04_station_runtime_poll(&a->radio))break;wait_us(a,1000);}if(a->tx.pending)(void)a04_native_nbl_abort(&a->tx,&a->session.diagnostic.bridge);}
  }
  RtlSecureZeroMemory(a->mpdu,sizeof(a->mpdu));RtlSecureZeroMemory(a->ethernet,sizeof(a->ethernet));RtlSecureZeroMemory(a->session.diagnostic.bridge.wlan.tx,sizeof(a->session.diagnostic.bridge.wlan.tx));
  if(s!=NDIS_STATUS_PENDING){NET_BUFFER_LIST_STATUS(send)=s;NdisMSendNetBufferListsComplete(a->adapter,send,0);}
  NdisAcquireSpinLock(&a->lock);a->send=NULL;NdisReleaseSpinLock(&a->lock);
 }else if(!a->closing&&!a->paused)(void)a04_station_runtime_poll(&a->radio);
 NdisAcquireSpinLock(&a->lock);
 if(a->next_request){a->request=a->next_request;a->next_request=NULL;a->cancelled=a->next_cancelled;a->next_cancelled=0;NdisReleaseSpinLock(&a->lock);goto again;}
 if(a->next_send){a->send=a->next_send;a->next_send=NULL;NdisReleaseSpinLock(&a->lock);goto again;}
 NdisFreeIoWorkItem(work);a->busy=0;a->cancelled=0;NdisSetEvent(&a->idle);NdisReleaseSpinLock(&a->lock);
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
 a04_os_security_clear(a);RtlSecureZeroMemory(a->early_rx,sizeof(a->early_rx));RtlSecureZeroMemory(a->mpdu,sizeof(a->mpdu));RtlSecureZeroMemory(a->ethernet,sizeof(a->ethernet));RtlSecureZeroMemory(a->rx_bytes,sizeof(a->rx_bytes));
 if(a->session.state==A04_SESSION_READY||a->session.state==A04_SESSION_QUARANTINED){if(a04_windows_session_stop(&a->session))a->uncertain=1;}
 if(a->irq.interrupt)(void)a04_windows_irq_remove(&a->irq);
 /* Do not free objects whose live MMIO/lease cannot be released. */
 if(a->uncertain||a->session.state==A04_SESSION_QUARANTINED)return;
 if(a->referenced&&a->parent.Header.InterfaceDereference)a->parent.Header.InterfaceDereference(a->parent.Header.Context);
 if(a->rx_nbl)NdisFreeNetBufferList(a->rx_nbl);if(a->rx_mdl)IoFreeMdl(a->rx_mdl);if(a->rx_pool)NdisFreeNetBufferListPool(a->rx_pool);
NdisFreeSpinLock(&a->lock);RtlSecureZeroMemory(a,sizeof(*a));NdisFreeMemory(a,0,0);
}
MINIPORT_INITIALIZE RuntimeInitialize;MINIPORT_HALT RuntimeHalt;MINIPORT_PAUSE RuntimePause;MINIPORT_RESTART RuntimeRestart;
MINIPORT_OID_REQUEST RuntimeOid;MINIPORT_CANCEL_OID_REQUEST RuntimeCancelOid;MINIPORT_SEND_NET_BUFFER_LISTS RuntimeSend;
MINIPORT_RETURN_NET_BUFFER_LISTS RuntimeReturn;MINIPORT_CANCEL_SEND RuntimeCancelSend;MINIPORT_SHUTDOWN RuntimeShutdown;
DRIVER_UNLOAD RuntimeUnload;
NDIS_STATUS RuntimeInitialize(NDIS_HANDLE adapter,NDIS_HANDLE context,PNDIS_MINIPORT_INIT_PARAMETERS params)
{
 struct a04_ndis_runtime *a;NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES attr;NET_BUFFER_LIST_POOL_PARAMETERS pool;NDIS_STATUS s;LARGE_INTEGER due;
 UNREFERENCED_PARAMETER(context);UNREFERENCED_PARAMETER(params);
 a=NdisAllocateMemoryWithTagPriority(adapter,sizeof(*a),TAG,NormalPoolPriority);if(!a)return NDIS_STATUS_RESOURCES;NdisZeroMemory(a,sizeof(*a));a->adapter=adapter;a->paused=1;a->power=TRUE;a->lookahead=1500;
 NdisAllocateSpinLock(&a->lock);NdisInitializeEvent(&a->idle);NdisSetEvent(&a->idle);KeInitializeTimer(&a->timer);KeInitializeDpc(&a->dpc,poll_dpc,a);
 NdisZeroMemory(&attr,sizeof(attr));attr.Header.Type=NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;attr.Header.Revision=1;attr.Header.Size=NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;attr.MiniportAdapterContext=a;attr.InterfaceType=NdisInterfaceInternal;
 s=NdisMSetMiniportAttributes(adapter,(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&attr);if(s!=NDIS_STATUS_SUCCESS)goto failed;
 NdisZeroMemory(&pool,sizeof(pool));pool.Header.Type=NDIS_OBJECT_TYPE_DEFAULT;pool.Header.Revision=1;pool.Header.Size=sizeof(pool);pool.ProtocolId=NDIS_PROTOCOL_ID_DEFAULT;pool.fAllocateNetBuffer=TRUE;pool.PoolTag=TAG;
 a->rx_pool=NdisAllocateNetBufferListPool(adapter,&pool);a->rx_mdl=IoAllocateMdl(a->rx_bytes,sizeof(a->rx_bytes),FALSE,FALSE,NULL);
 if(!a->rx_pool||!a->rx_mdl){s=NDIS_STATUS_RESOURCES;goto failed;}MmBuildMdlForNonPagedPool(a->rx_mdl);a->rx_nbl=NdisAllocateNetBufferAndNetBufferList(a->rx_pool,0,0,a->rx_mdl,0,sizeof(a->rx_bytes));
 if(!a->rx_nbl){s=NDIS_STATUS_RESOURCES;goto failed;}
 if(!NT_SUCCESS(query_parent(a))||hardware_start(a)){s=NDIS_STATUS_ADAPTER_NOT_READY;goto failed;}
 a->tx.adapter=adapter;s=a04_ndis_attributes(a);if(s!=NDIS_STATUS_SUCCESS)goto failed;
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
VOID RuntimeCancelOid(NDIS_HANDLE p,PVOID id){struct a04_ndis_runtime *a=p;NdisAcquireSpinLock(&a->lock);if(a->request&&a->request->RequestId==id)a->cancelled=1;if(a->next_request&&a->next_request->RequestId==id)a->next_cancelled=1;NdisReleaseSpinLock(&a->lock);}
VOID RuntimeSend(NDIS_HANDLE p,PNET_BUFFER_LIST list,NDIS_PORT_NUMBER port,ULONG flags)
{
 struct a04_ndis_runtime *a=p;UNREFERENCED_PARAMETER(port);
 NdisAcquireSpinLock(&a->lock);if(a->busy&&!a->closing&&!a->paused&&!a->next_send&&!NET_BUFFER_LIST_NEXT_NBL(list)){a->next_send=list;NdisReleaseSpinLock(&a->lock);return;}if(!a->busy&&!a->closing&&!a->paused&&!NET_BUFFER_LIST_NEXT_NBL(list)){a->send=list;if(!queue_work(a)){NdisReleaseSpinLock(&a->lock);return;}a->send=NULL;}NdisReleaseSpinLock(&a->lock);
 for(PNET_BUFFER_LIST n=list;n;n=NET_BUFFER_LIST_NEXT_NBL(n))NET_BUFFER_LIST_STATUS(n)=NDIS_STATUS_ADAPTER_NOT_READY;
 NdisMSendNetBufferListsComplete(a->adapter,list,(flags&NDIS_SEND_FLAGS_DISPATCH_LEVEL)?NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL:0);
}
VOID RuntimeReturn(NDIS_HANDLE p,PNET_BUFFER_LIST n,ULONG flags){UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(n);UNREFERENCED_PARAMETER(flags);} /* RESOURCES receive only */
VOID RuntimeCancelSend(NDIS_HANDLE p,PVOID id){UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(id);} /* bounded synchronous submission, completed or aborted by worker */
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
 NDIS_MINIPORT_DRIVER_CHARACTERISTICS c;NdisZeroMemory(&c,sizeof(c));c.Header.Type=NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;c.Header.Revision=2;c.Header.Size=NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;c.MajorNdisVersion=6;c.MinorNdisVersion=30;c.MajorDriverVersion=0;c.MinorDriverVersion=2;
 c.InitializeHandlerEx=RuntimeInitialize;c.HaltHandlerEx=RuntimeHalt;c.PauseHandler=RuntimePause;c.RestartHandler=RuntimeRestart;c.OidRequestHandler=RuntimeOid;c.CancelOidRequestHandler=RuntimeCancelOid;c.SendNetBufferListsHandler=RuntimeSend;c.ReturnNetBufferListsHandler=RuntimeReturn;c.CancelSendHandler=RuntimeCancelSend;c.ShutdownHandlerEx=RuntimeShutdown;c.UnloadHandler=RuntimeUnload;c.DevicePnPEventNotifyHandler=RuntimePnp;
 return NdisMRegisterMiniportDriver(p,path,NULL,&c,&driver_handle);
}
