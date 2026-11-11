/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native 802.11 OS supplicant boundary, not an in-driver WPA supplicant.
 * MT6630 gen3: CMD_802_11_KEY/CID7, EVENT_ID_ADD_PKEY_DONE/0x24;
 * wlan_oid.c wlanoidSetAddKey/RemoveKey, nic_rx.c, nic_tx.c. */
#include "ndis_runtime.h"
#include "native_privacy.h"
#include "sha256.h"
#include "native_tx.h"
#include "native_frame.h"
static int same(const uint8_t *a,const uint8_t *b,unsigned n){unsigned d=0;for(unsigned i=0;i<n;i++)d|=a[i]^b[i];return !d;}
static int fail(struct a04_ndis_runtime *a){a->os_security.failed=1;a->os_security.port_open=0;return -1;}
void a04_os_security_clear(struct a04_ndis_runtime *a)
{a->receive_epoch++;RtlSecureZeroMemory(&a->os_security,sizeof(a->os_security));RtlSecureZeroMemory(&a->security,sizeof(a->security));}
static int cipher_active(const struct a04_ndis_runtime *a)
{return a->os_security.auth==DOT11_AUTH_ALGO_RSNA_PSK&&a->os_security.unicast==DOT11_CIPHER_ALGO_CCMP&&a->os_security.multicast==DOT11_CIPHER_ALGO_CCMP;}
static unsigned exemption(struct a04_os_security *s,unsigned ethertype,unsigned group)
{
 for(unsigned i=0;i<s->exemption_count;i++){
  DOT11_PRIVACY_EXEMPTION *e=&s->exemptions[i];
  if(a04_native_privacy_protocol(&e->usEtherType)==ethertype&&(e->usExemptionPacketType==DOT11_EXEMPT_BOTH||e->usExemptionPacketType==(group?DOT11_EXEMPT_MULTICAST:DOT11_EXEMPT_UNICAST)))return e->usExemptionActionType;
 }
 return DOT11_EXEMPT_NO_EXEMPTION;
}
int a04_os_security_event(struct a04_ndis_runtime *a,const uint8_t *p,unsigned n)
{
 struct a04_wlan_event e;struct a04_os_security *s=&a->os_security;
 if(a04_wlan_event_parse(p,n,&e))return fail(a);
 if(e.id!=0x24)return 0;
 if(!s->ptk_pending||e.length!=8||e.payload[0]!=a->radio.station.local.bss||e.payload[1]||!same(e.payload+2,a->radio.station.ap.mac,6)){
  a04_ndis_connection_trace(a,A04_CT_KEY_EVENT,A04_CP_RESULT,A04_CP_BACKEND,-1,0);return fail(a);}
 a04_ndis_connection_trace(a,A04_CT_KEY_EVENT,A04_CP_RESULT,A04_CP_BACKEND,0,0);
 s->ptk_pending=0;s->ptk.valid=1;return 0;
}
static NDIS_STATUS install(struct a04_ndis_runtime *a,unsigned pairwise,unsigned id,int remove,const uint8_t *key,const uint8_t rsc[6])
{
 struct a04_os_security *s=&a->os_security;struct a04_os_key *slot=pairwise?&s->ptk:&s->gtk[id];
 struct a04_wmt_bridge *b=a->radio.bridge;uint8_t command[64],digest[32];uint64_t pn=0,deadline;unsigned wlan=slot->wlan,epoch=a->receive_epoch;int e;NDIS_STATUS status=NDIS_STATUS_FAILURE;
 /* WDK OID_DOT11_CIPHER_DEFAULT_KEY: deleting an already absent key
  * succeeds even during disconnected/reset cleanup. OID shape/id/cipher
  * validation occurs before install(). No FW operation is needed. */
 /* valid=0 is not proof of absence while a key transaction is pending
  * or its submission/completion is uncertain. Those never become no-ops. */
 if(slot->uncertain||s->ptk_pending)return NDIS_STATUS_DOT11_MEDIA_IN_USE;
 if(remove&&!slot->valid)return NDIS_STATUS_SUCCESS;
 if(!a->radio.connected||!cipher_active(a)||s->failed||a->tx.pending)return NDIS_STATUS_DOT11_MEDIA_IN_USE;
 RtlZeroMemory(command,sizeof(command));RtlZeroMemory(digest,sizeof(digest));
 if(!remove){
  if(a04_sha256(key,16,digest))goto done;
  if(slot->seen&&same(slot->fingerprint,digest,32)){
   /* Duplicate key OIDs must never reinstall the same TK or reset a PN.
    * A deleted key cannot be resurrected in the same association epoch. */
   status=slot->valid?NDIS_STATUS_SUCCESS:NDIS_STATUS_INVALID_DATA;goto done;
  }
 }
 if(pairwise)wlan=a->radio.station.local.wlan;
 else if(!slot->seen){
  unsigned occupied=0;for(unsigned i=0;i<4;i++)if(s->gtk[i].seen&&s->gtk[i].wlan==a->radio.station.local.bmc)occupied=1;
  if(!occupied)wlan=a->radio.station.local.bmc;
  else{for(wlan=19;wlan<=27&&(a->radio.slots.wtbl&(1u<<wlan));wlan++){}if(wlan>27){status=NDIS_STATUS_RESOURCES;goto done;}a->radio.slots.wtbl|=1u<<wlan;}
 }
 command[0]=(uint8_t)!remove;command[1]=command[2]=(uint8_t)pairwise;
 for(unsigned i=0;i<6;i++)command[4+i]=a->radio.station.ap.mac[i];
 command[10]=(uint8_t)a->radio.station.local.bss;command[11]=4;command[12]=(uint8_t)id;command[13]=(uint8_t)(remove?0:16);command[14]=(uint8_t)wlan;
 if(!remove){for(unsigned i=0;i<16;i++)command[16+i]=key[i];for(unsigned i=0;i<6;i++){command[48+i]=rsc[i];pn|=(uint64_t)rsc[i]<<(8*i);}}
 /* Drain old events before publishing a new key transaction under this owner. */
 if(a04_ndis_poll(a)){if(a->receive_epoch!=epoch)goto done;goto uncertain;}
 if(a->receive_epoch!=epoch||!a->radio.connected)goto done;
 slot->valid=0;slot->uncertain=1;slot->wlan=wlan;if(!remove)for(unsigned i=0;i<16;i++)slot->pn[i]=pn;if(pairwise&&!remove)s->ptk_pending=1;
 a04_ndis_connection_trace(a,A04_CT_KEY_CID7,A04_CP_BEGIN,A04_CP_UNKNOWN,0,pairwise?1:2);
 e=a04_wlan_set(&b->wlan,7,command,64,NULL);
 a04_ndis_connection_trace(a,A04_CT_KEY_CID7,A04_CP_RESULT,A04_CP_BACKEND,e,pairwise?1:2);
 RtlSecureZeroMemory(command,sizeof(command));RtlSecureZeroMemory(b->wlan.tx,sizeof(b->wlan.tx));
 if(e)goto uncertain;
 if(pairwise&&!remove){
  deadline=a->radio.now(a)+2000000u;
  while(s->ptk_pending&&!a->closing&&!a->cancelled&&a->radio.now(a)<deadline){if(a04_ndis_poll(a)){if(a->receive_epoch!=epoch)goto done;goto uncertain;}if(a->receive_epoch!=epoch||!a->radio.connected)goto done;a->radio.wait(a,1000);}
  if(s->ptk_pending){a04_ndis_connection_trace(a,A04_CT_KEY_EVENT,A04_CP_RESULT,A04_CP_BACKEND,a->closing||a->cancelled?A04_WLAN_STATE:A04_WLAN_TIMEOUT,1);goto uncertain;}
 }
 if(a->receive_epoch!=epoch||!a->radio.connected)goto done;
 if(remove){slot->valid=0;slot->uncertain=0;if(pairwise)s->port_open=0;status=NDIS_STATUS_SUCCESS;goto done;}
 /* Group-key completion follows gen3 noNeedResp/full HIF submission; this
  * is deliberately not reported as a fabricated firmware GTK ACK. */
 slot->seen=slot->valid=1;slot->uncertain=0;slot->wlan=wlan;for(unsigned i=0;i<32;i++)slot->fingerprint[i]=digest[i];
 status=NDIS_STATUS_SUCCESS;goto done;
uncertain:fail(a);a->radio.failed=-1;b->failed=1;
done:a04_ndis_connection_trace(a,A04_CT_KEY_CID7,A04_CP_RETURN_PREPARED,A04_CP_NDIS,status,pairwise?1:2);RtlSecureZeroMemory(command,sizeof(command));RtlSecureZeroMemory(digest,sizeof(digest));return status;
}
static NDIS_STATUS decode_key(struct a04_ndis_runtime *a,unsigned pairwise,unsigned id,int remove,const uint8_t *p,unsigned n)
{
 DOT11_KEY_ALGO_CCMP h;uint8_t key[16],rsc[6];NDIS_STATUS status;unsigned base=FIELD_OFFSET(DOT11_KEY_ALGO_CCMP,ucCCMPKey);
 if(remove)return install(a,pairwise,id,1,NULL,NULL);
 if(n!=base+16)return NDIS_STATUS_INVALID_LENGTH;
 NdisZeroMemory(&h,sizeof(h));NdisMoveMemory(&h,p,base);if(h.ulCCMPKeyLength!=16)return NDIS_STATUS_INVALID_DATA;
 NdisMoveMemory(key,p+base,16);NdisMoveMemory(rsc,h.ucIV48Counter,6);
 status=install(a,pairwise,id,0,key,rsc);RtlSecureZeroMemory(key,sizeof(key));RtlSecureZeroMemory(rsc,sizeof(rsc));RtlSecureZeroMemory(&h,sizeof(h));return status;
}
NDIS_STATUS a04_os_security_oid(struct a04_ndis_runtime *a,PNDIS_OID_REQUEST q)
{
 ULONG oid=q->DATA.SET_INFORMATION.Oid,n=q->DATA.SET_INFORMATION.InformationBufferLength;const uint8_t *p=q->DATA.SET_INFORMATION.InformationBuffer;NDIS_STATUS status;
 if(q->RequestType!=NdisRequestSetInformation)return NDIS_STATUS_NOT_SUPPORTED;
 q->DATA.SET_INFORMATION.BytesRead=q->DATA.SET_INFORMATION.BytesNeeded=0;
 if(!p&&n)return NDIS_STATUS_INVALID_DATA;
 if(oid==OID_DOT11_CIPHER_KEY_MAPPING_KEY){
  DOT11_BYTE_ARRAY arr;DOT11_CIPHER_KEY_MAPPING_KEY_VALUE entry;unsigned base=FIELD_OFFSET(DOT11_BYTE_ARRAY,ucBuffer),head=FIELD_OFFSET(DOT11_CIPHER_KEY_MAPPING_KEY_VALUE,ucKey);
  if(n<base)return NDIS_STATUS_INVALID_LENGTH;NdisZeroMemory(&arr,sizeof(arr));NdisMoveMemory(&arr,p,base);
  if(arr.Header.Type!=NDIS_OBJECT_TYPE_DEFAULT||arr.Header.Revision!=1||arr.Header.Size!=sizeof(arr)||arr.uNumOfBytes>n-base||arr.uTotalNumOfBytes<arr.uNumOfBytes||arr.uNumOfBytes<head)return NDIS_STATUS_INVALID_DATA;
  NdisZeroMemory(&entry,sizeof(entry));NdisMoveMemory(&entry,p+base,head);
  if(entry.usKeyLength!=arr.uNumOfBytes-head||entry.AlgorithmId!=DOT11_CIPHER_ALGO_CCMP||entry.Direction!=DOT11_DIR_BOTH||entry.bStatic||!same(entry.PeerMacAddr,a->radio.station.ap.mac,6))return NDIS_STATUS_INVALID_DATA;
  status=decode_key(a,1,0,entry.bDelete,p+base+head,entry.usKeyLength);
  if(status==NDIS_STATUS_SUCCESS)q->DATA.SET_INFORMATION.BytesRead=base+arr.uNumOfBytes;return status;
 }
 if(oid==OID_DOT11_CIPHER_DEFAULT_KEY){
  DOT11_CIPHER_DEFAULT_KEY_VALUE entry;unsigned base=FIELD_OFFSET(DOT11_CIPHER_DEFAULT_KEY_VALUE,ucKey),zero=0,ff=0;
  if(n<base)return NDIS_STATUS_INVALID_LENGTH;NdisZeroMemory(&entry,sizeof(entry));NdisMoveMemory(&entry,p,base);
  if(entry.Header.Type!=NDIS_OBJECT_TYPE_DEFAULT||entry.Header.Revision!=1||entry.Header.Size!=sizeof(entry)||entry.uKeyIndex>3||entry.AlgorithmId!=DOT11_CIPHER_ALGO_CCMP||entry.bStatic||entry.usKeyLength!=n-base)return NDIS_STATUS_INVALID_DATA;
  for(unsigned i=0;i<6;i++){zero|=entry.MacAddr[i];ff|=entry.MacAddr[i]^255;}
  if(zero&&ff)return NDIS_STATUS_NOT_SUPPORTED;
  /* The outer radio-failure gate delegates only this SET OID. Keep every
   * hardware-changing or uncertain operation blocked, but permit the WDK
   * required repeat-delete no-op after full input validation. */
  if(a->power&&a->radio.failed){
   const struct a04_os_key *slot=&a->os_security.gtk[entry.uKeyIndex];
   if(!entry.bDelete||slot->valid||slot->uncertain||a->os_security.ptk_pending)return NDIS_STATUS_ADAPTER_NOT_READY;
  }
  status=decode_key(a,0,entry.uKeyIndex,entry.bDelete,p+base,entry.usKeyLength);if(status==NDIS_STATUS_SUCCESS)q->DATA.SET_INFORMATION.BytesRead=n;return status;
 }
 if(oid==OID_DOT11_PORT_STATE_NOTIFICATION){
  DOT11_PORT_STATE_NOTIFICATION v;if(n!=sizeof(v))return NDIS_STATUS_INVALID_LENGTH;NdisMoveMemory(&v,p,sizeof(v));
  if(v.Header.Type!=NDIS_OBJECT_TYPE_DEFAULT||v.Header.Revision!=1||v.Header.Size!=sizeof(v)||!a->radio.connected||!same(v.PeerMac,a->radio.station.ap.mac,6))return NDIS_STATUS_INVALID_DATA;
  if(v.bOpen&&(!a->os_security.ptk.valid||a->os_security.ptk_pending||a->os_security.failed))return NDIS_STATUS_ADAPTER_NOT_READY;
  a->os_security.port_open=!!v.bOpen;q->DATA.SET_INFORMATION.BytesRead=n;return NDIS_STATUS_SUCCESS;
 }
 return NDIS_STATUS_NOT_SUPPORTED;
}
NDIS_STATUS a04_os_security_send(struct a04_ndis_runtime *a,PNET_BUFFER_LIST nbl,uint64_t now,uint64_t deadline)
{
 PNET_BUFFER nb;void *view;ULONG length;size_t bytes=0;struct a04_wlan_host *h=&a->radio.bridge->wlan;struct a04_os_security *s=&a->os_security;NDIS_STATUS result=NDIS_STATUS_INVALID_DATA;unsigned eapol=0,exempt,protect,pid,tid;int e;struct a04_native_frame f;
 if(!cipher_active(a)||s->failed||a->tx.pending||NET_BUFFER_LIST_NEXT_NBL(nbl))return result;
 nb=NET_BUFFER_LIST_FIRST_NB(nbl);if(!nb||NET_BUFFER_NEXT_NB(nb))return result;length=NET_BUFFER_DATA_LENGTH(nb);
 if(length<32||length>sizeof(a->mpdu))return NDIS_STATUS_INVALID_LENGTH;
 view=NdisGetDataBuffer(nb,length,a->mpdu,1,0);if(!view)return NDIS_STATUS_RESOURCES;if(view!=a->mpdu)NdisMoveMemory(a->mpdu,view,length);
 if(a04_native_frame_parse(a->mpdu,length,1,&f)||(a->mpdu[1]&0x3c))goto done;a->mpdu[1]&=~0x40u;
 if(a04_native_tx_to_ethernet(a->mpdu,length,a->radio.station.ap.mac,a->radio.station.local.mac,a->ethernet,sizeof(a->ethernet),&bytes))goto done;
 NDIS_NET_BUFFER_LIST_8021Q_INFO priority;priority.Value=NET_BUFFER_LIST_INFO(nbl,Ieee8021QNetBufferListInfo);
 if(f.qos&&priority.TagHeader.UserPriority&&priority.TagHeader.UserPriority!=f.tid)goto done;
 tid=f.qos?f.tid:(unsigned)priority.TagHeader.UserPriority;
 eapol=a04_wlan_be16(a->ethernet+12)==0x888e;
 if(eapol)a04_ndis_connection_trace(a,A04_CT_EAPOL_TX,A04_CP_BEGIN,A04_CP_UNKNOWN,0,0);
 if(eapol){static const uint8_t pae[6]={1,0x80,0xc2,0,0,3};if(!same(a->ethernet,a->radio.station.ap.mac,6)&&!same(a->ethernet,pae,6))goto done;}
 {PDOT11_EXTSTA_SEND_CONTEXT oob=NET_BUFFER_LIST_INFO(nbl,MediaSpecificInformation);
  if(!oob||oob->Header.Type!=NDIS_OBJECT_TYPE_DEFAULT||oob->Header.Revision!=1||oob->Header.Size<sizeof(*oob)||oob->usExemptionActionType>DOT11_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE)goto done;
  exempt=oob->usExemptionActionType;
 }
 if(!eapol&&(!s->port_open||!s->ptk.valid||exempt!=DOT11_EXEMPT_NO_EXEMPTION))goto done;
 if(eapol&&exempt==DOT11_EXEMPT_NO_EXEMPTION&&!s->ptk.valid)goto done;
 protect=s->ptk.valid&&!(eapol&&exempt==DOT11_EXEMPT_ALWAYS);
 if(deadline<=now||deadline-now>1000000u)goto done;
 /* Matching TX_DONE is not a page return. Drain actual WTQCR/FFA
  * credits before another encrypted data submission, just like OPEN. */
 unsigned pages=((unsigned)bytes+28+127)/128;unsigned *free_pages=h->native_be?&h->tc1_free:&h->tc0_free;
 for(unsigned tries=0;!eapol&&*free_pages<pages;tries++){
  if(a->closing||a->cancelled){result=NDIS_STATUS_REQUEST_ABORTED;goto done;}
  if(tries==100||a04_wlan_resources(h)){result=NDIS_STATUS_FAILURE;fail(a);goto done;}
  if(*free_pages<pages)a->radio.wait(a,1000);
 }
 pid=a->tx.state.next_pid%127+1;
 a04_wlan_zero(h->tx,28);a04_wmt_put16(h->tx,(unsigned)bytes+28);h->tx[2]=20;h->tx[3]=(uint8_t)(eapol?0x88:(h->native_be?8:0));h->tx[4]=(uint8_t)a->radio.station.local.wlan;h->tx[5]=0x90;h->tx[6]=(uint8_t)((protect?0x80:0)|(tid<<4));h->tx[7]=(uint8_t)(a->radio.station.local.own<<2);a04_wmt_put16(h->tx+12,1u<<11);
 if(eapol){unsigned all,basic;if(a04_wlan_rate_mask(&a->radio.station.ap,&all,&basic))goto done;h->tx[11]=0x80;a04_wmt_put16(h->tx+26,a04_wlan_basic_rate_code(basic)<<2);}
 h->tx[20]=(uint8_t)pid;h->tx[21]=2;for(unsigned i=0;i<bytes;i++)h->tx[28+i]=a->ethernet[i];
 e=a04_wlan_send(h,(unsigned)bytes+28,eapol?1:(h->native_be?3:0));if(e){fail(a);result=NDIS_STATUS_FAILURE;goto done;}
 a->tx.state.pending=1;a->tx.state.pid=a->tx.state.next_pid=pid;a->tx.state.eapol_kind=0;a->tx.state.deadline_us=deadline;a->tx.pending=nbl;result=NDIS_STATUS_PENDING;
done:if(eapol){a04_ndis_connection_trace(a,A04_CT_EAPOL_TX,A04_CP_RESULT,A04_CP_NDIS,result,0);if(result==NDIS_STATUS_PENDING)a->eapol_tx_observed=1;}RtlSecureZeroMemory(a->mpdu,sizeof(a->mpdu));RtlSecureZeroMemory(a->ethernet,sizeof(a->ethernet));RtlSecureZeroMemory(h->tx,sizeof(h->tx));return result;
}
static NDIS_STATUS security_receive_impl(struct a04_ndis_runtime *a,const uint8_t *p,ULONG n,struct a04_native_hif_rx *rx)
{
 struct a04_os_security *s=&a->os_security;struct a04_os_key *key;NDIS_STATUS status;unsigned ethertype,group,exempt;
 status=a04_native_hif_rx_decode_owned(p,n,a->radio.bridge->regulatory.permitted,a->radio.station.local.mac,a->radio.station.ap.mac,a,a04_ndis_rate_lookup,1,a->rx_bytes,sizeof(a->rx_bytes),rx);
 a04_ndis_connection_trace(a,A04_CT_RX_DECODE,A04_CP_RESULT,A04_CP_NDIS,status,n>=7&&(p[6]&0x80)?2:1);
 if(status!=NDIS_STATUS_SUCCESS)return status;
 if(!cipher_active(a))return NDIS_STATUS_NOT_SUPPORTED;
 group=rx->mpdu[4]&1;ethertype=a04_wlan_be16(rx->mpdu+rx->header+6);exempt=exemption(s,ethertype,group);
 if(ethertype==0x888e&&(group||!same(rx->mpdu+16,a->radio.station.ap.mac,6)))return NDIS_STATUS_NOT_SUPPORTED;
 if(rx->cipher==4){
  key=group?&s->gtk[rx->key]:&s->ptk;
  if(!key->valid||rx->wlan!=key->wlan||(!group&&rx->key)||rx->pn<=key->pn[rx->tid])return NDIS_STATUS_NOT_SUPPORTED;
  if(exempt==DOT11_EXEMPT_ALWAYS)return NDIS_STATUS_NOT_SUPPORTED;
 }else{
  key=NULL;if(rx->wlan!=a->radio.station.local.wlan||group||ethertype!=0x888e||s->exclude_unencrypted&&exempt==DOT11_EXEMPT_NO_EXEMPTION)return NDIS_STATUS_NOT_SUPPORTED;
  if(exempt==DOT11_EXEMPT_ALWAYS){}else if(exempt==DOT11_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE&&!s->ptk.valid){}else return NDIS_STATUS_NOT_SUPPORTED;
 }
 if(ethertype!=0x888e&&!s->port_open)return NDIS_STATUS_NOT_SUPPORTED;
 if(rx->length>sizeof(a->rx_bytes))return NDIS_STATUS_INVALID_LENGTH;
 /* Accept only HW-verified, already stripped raw LLC/SNAP MPDUs. Do not
  * guess IV/MIC lengths or perform software crypto without actual metadata. */
 /* Borrow HIF until the final owned RX slot copies/normalizes it once. */
 if(key)key->pn[rx->tid]=rx->pn;
 return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS a04_os_security_receive(struct a04_ndis_runtime *a,const uint8_t *p,ULONG n,struct a04_native_hif_rx *rx)
{
 NDIS_STATUS status;NdisZeroMemory(rx,sizeof(*rx));
 status=security_receive_impl(a,p,n,rx);
 if(rx->mpdu&&rx->length>=rx->header+8&&a04_wlan_be16(rx->mpdu+rx->header+6)==0x888e)
  a04_ndis_connection_trace(a,A04_CT_EAPOL_RX,A04_CP_RESULT,A04_CP_NDIS,status,0);
 return status;
}
