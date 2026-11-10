/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Single PASSIVE_LEVEL SDIO owner. Consume actual WHISR/RX events, never
 * synthesize scan completion, association response or STA activation. */
#include "station_runtime.h"
#include "rsn_policy.h"
#include "assoc_capability.h"
#include "checkpoint_wire.h"
#define TRACE(s,k,v,p) do{if(r->trace)r->trace(r->context,s,A04_CP_RESULT,k,(uint32_t)(v),(uint32_t)(p));}while(0)
static int fail(struct a04_station_runtime *r,int e)
{r->failed=e?e:-1;r->connecting=r->connected=0;return r->failed;}
int a04_station_runtime_init(struct a04_station_runtime *r,struct a04_wmt_bridge *b)
{
 struct a04_wlan_local local;
 if(!r||!b||r->initialized||!r->now||!r->wait||!b->calibration_submitted||b->failed)return -1;
 /* One whole-combo owner after cold reset; no second BSS/STA/FM/BT client. */
 if(a04_wlan_slots_take(&r->slots,b->regulatory.mac,b->regulatory.permitted,b->regulatory.active,&local)||
    a04_wmt_bridge_station_setup(b,&r->station,&local))return fail(r,-1);
 r->bridge=b;r->initialized=1;return 0;
}
static int packet(struct a04_station_runtime *r,const uint8_t *p,unsigned n)
{
 struct a04_wmt_bridge *b=r->bridge;struct a04_wlan_rx_frame frame;uint64_t now=r->now(r->context);int e;
 r->rx_bytes=n;r->rx_descriptor=n>=4?a04_wmt_le16(p+2):0;r->rx_error=0;r->rx_phase=1;
 if(n>=8&&a04_wmt_le16(p+2)==0xe000){
  struct a04_wlan_event v;e=a04_wlan_event_parse(p,n,&v);if(e){r->rx_error=e;return fail(r,e);}
  /* A source-backed host cancellation retires this scan sequence until
   * an actual WLAN firmware restart, not merely a host state clear. */
  if(v.id==0x0d&&v.length>=4&&(r->cancelled_scan_sequences[v.payload[0]>>5]&(1u<<(v.payload[0]&31))))return 0;
  if(v.id==0x0d&&b->scan.pending){
   e=a04_wmt_bridge_scan_event(b,p,n);if(e)return fail(r,e);
   /* Native scan confirmation consumes bridge.complete, not the observed
    * BSS cache. Retain this actual matching firmware completion for the
    * station selector; never manufacture scan success at connect time. */
   r->station.scan=b->scan;
  }
  else if(r->connecting){e=a04_wmt_bridge_station_event(b,&r->station,p,n,now);
   if(v.id==0x10)TRACE(A04_CT_CHANNEL,A04_CP_BACKEND,e,r->phase);
   if(v.id==0x0c)TRACE(A04_CT_STA,A04_CP_BACKEND,e,r->phase);
   if(e&&e!=A04_WLAN_UNSUPPORTED)return fail(r,e);}
  if(r->event&&r->event(r->context,p,n,now))return fail(r,-1);
  return 0;
 }
 if(r->data){e=r->data(r->context,p,n);if(e!=A04_WLAN_UNSUPPORTED)return e?fail(r,e):0;}
 r->rx_phase=2;
 e=a04_wlan_rx_frame_parse(p,n,&frame);if(e==A04_WLAN_UNSUPPORTED)return 0;if(e){r->rx_error=e;return fail(r,e);}
 if(!frame.length)return fail(r,-1);
 if((frame.data[0]==0x80||frame.data[0]==0x50)&&b->scan.pending){
  struct a04_wlan_ap ap;unsigned i;
  r->rx_phase=3;e=a04_wlan_ap_parse(frame.data,frame.length,frame.channel,&ap);
  /* Linux gen3 scanProcessBeaconAndProbeResp/scanAddToBssDesc reject a
   * candidate without aborting the scan. An untrusted/mismatched beacon
   * must not poison the combo owner. Keep the strict parser and discard
   * its output; only the actual firmware scan-done event completes scan. */
  if(e){
   if(!r->rejected_aps){r->rejected_ap_error=e;r->rejected_ap_bytes=n;r->rejected_ap_descriptor=r->rx_descriptor;}
   if(r->rejected_aps!=0xffffffffu)r->rejected_aps++;
   return 0;
  }
  if(!(b->regulatory.permitted&(1u<<ap.channel))||(r->exclude_bssid&&r->exclude_bssid(r->context,ap.mac)))return 0;
  if(frame.length-36>sizeof(r->bss[0].ies))return 0;
  for(i=0;i<r->bss_count;i++)if(a04_wlan_equal(r->bss[i].ap.mac,ap.mac,6))break;
  if(i==r->bss_count){if(i==A04_BSS_MAX)return 0;r->bss_count++;}
  r->bss[i].ap=ap;r->bss[i].host_timestamp=r->system_time?r->system_time(r->context):0;r->bss[i].timestamp=(uint64_t)a04_wmt_get32(frame.data+24)|((uint64_t)a04_wmt_get32(frame.data+28)<<32);
  if(frame.length>1024)return 0;r->bss[i].frame_length=frame.length;for(unsigned j=0;j<frame.length;j++)r->bss[i].frame[j]=frame.data[j];
  {unsigned groups=(a04_wmt_le16(p+2)>>9)&15;if(groups&4){unsigned off=16+((groups&8)?16:0)+((groups&1)?16:0)+((groups&2)?8:0);unsigned rcpi=p[off+9];if(rcpi<=220){r->bss[i].ap.rcpi_valid=1;r->bss[i].ap.rcpi=rcpi;}}}
  r->bss[i].ie_length=frame.length-36;
  for(unsigned j=0;j<frame.length-36;j++)r->bss[i].ies[j]=frame.data[36+j];
  return 0;
 }
 if(r->connecting&&frame.length>=24&&a04_wlan_equal(frame.data+10,r->station.ap.mac,6)){
  if(frame.data[0]==0xb0&&r->phase==1){
   e=a04_wmt_bridge_station_auth_rx(b,&r->station,frame.data,frame.length,now);
   TRACE(A04_CT_AUTH_RX,A04_CP_BACKEND,e,r->phase);
   if(frame.length>=30)TRACE(A04_CT_AUTH_RX,A04_CP_OBSERVATION,a04_wmt_le16(frame.data+28),0);
   if(!e){unsigned count=0,capability=0;const uint8_t *ie=NULL;if(r->station.secured)ie=a04_wlan_rsn_selected(&count);
    e=a04_mt6630_assoc_capability(r->station.ap.capability,r->station.secured,&capability);
    if(!e)e=a04_wmt_bridge_station_assoc(b,&r->station,capability,10,ie,count,now);
    TRACE(A04_CT_ASSOC_TX,A04_CP_BACKEND,e,r->phase);}
   if(e)return fail(r,e);r->assoc_request_length=a04_wmt_le16(b->wlan.tx)-28;
   if(r->assoc_request_length>sizeof(r->assoc_request))return fail(r,-1);
   for(unsigned i=0;i<r->assoc_request_length;i++)r->assoc_request[i]=b->wlan.tx[28+i];r->phase=2;
  }else if(frame.data[0]==0x10&&r->phase==2){
   e=a04_wmt_bridge_station_assoc_rx(b,&r->station,frame.data,frame.length,r->station.secured,now);
   TRACE(A04_CT_ASSOC_RX,A04_CP_BACKEND,e,r->phase);
   if(frame.length>=28)TRACE(A04_CT_ASSOC_RX,A04_CP_OBSERVATION,a04_wmt_le16(frame.data+26),0);
   if(e)return fail(r,e);if(frame.length>sizeof(r->assoc_response))return fail(r,-1);
   r->assoc_response_length=frame.length;for(unsigned i=0;i<frame.length;i++)r->assoc_response[i]=frame.data[i];r->phase=3;
  }
 }

 return 0;
}
int a04_station_runtime_poll(struct a04_station_runtime *r)
{
 struct a04_wmt_bridge *b;uint32_t status,lengths;int e;unsigned port,packets;
 if(!r||!r->initialized||r->failed)return -1;b=r->bridge;
 e=a04_wmt_bridge_poll_wlan_status(b,&status);if(e)return fail(r,e);
 /* gen3 mt6630_reg.h: bit7 is FW_OWN_BACK, a normal notification.
  * WHISR is read-clear. It carries no TX credit and does not itself prove
  * host ownership; leave the WHLPCR handshake and all fault bits intact. */
 if(status&~0x87u)return fail(r,A04_WLAN_STATUS);
 if(status&1u){e=a04_wlan_resources(&b->wlan);if(e)return fail(r,e);}
 status|=r->rx_pending;r->rx_pending=0;b->pending_wlan_status=status;
 for(port=0;port<2;port++)if(status&(2u<<port)){
  for(packets=0;;){const uint8_t *p;unsigned n;
   e=a04_wlan_read(&b->wlan,0x90,&lengths);if(e)return fail(r,e);
   if(!((lengths>>(16*port))&0xffffu))break;
   if(packets==32){r->rx_pending|=2u<<port;break;}
   packets++;
   r->rx_phase=4;
   e=a04_wmt_bridge_receive(b,port,1,&p,&n);if(e){r->rx_error=e;return fail(r,e);}
   e=packet(r,p,n);a04_wlan_secret_clear(b->wlan.rx,sizeof(b->wlan.rx));if(e)return fail(r,e);
  }
 }
 if(status&&a04_wmt_bridge_ack_wlan_status(b,status))return fail(r,-1);
 if(r->connecting&&!r->station.channel_pending&&r->phase==0){
  e=a04_wmt_bridge_station_auth(b,&r->station,r->now(r->context));TRACE(A04_CT_AUTH_TX,A04_CP_BACKEND,e,r->phase);if(e)return fail(r,e);r->phase=1;
 }
 if(r->connecting&&r->station.sta_active){r->connecting=0;r->connected=1;}
 return 0;
}
int a04_station_runtime_connect(struct a04_station_runtime *r,const uint8_t *ssid,unsigned length)
{
 unsigned i;uint64_t now,deadline;int e;
 if(!r||!r->initialized||r->failed||r->connected||r->connecting||!ssid||!length||length>32||r->bridge->scan.pending)return -1;
 for(i=0;i<r->bss_count;i++)if(!(r->exclude_bssid&&r->exclude_bssid(r->context,r->bss[i].ap.mac))&&(!r->selected_bssid_set||a04_wlan_equal(r->selected_bssid,r->bss[i].ap.mac,6))&&r->bss[i].ap.ssid_length==length&&a04_wlan_equal(r->bss[i].ap.ssid,ssid,length))break;
 if(i==r->bss_count)return A04_WLAN_UNSUPPORTED;
 e=a04_os_candidate_policy(&r->bss[i].ap,r->station.local.active_channels,r->desired_secured);if(e)return e;
 now=r->now(r->context);deadline=now+5000000u;
 e=a04_wmt_bridge_station_select(r->bridge,&r->station,&r->bss[i].ap,r->desired_secured,1,5000,now,deadline);TRACE(A04_CT_SELECT,A04_CP_BACKEND,e,r->phase);if(e)return fail(r,e);
 r->connecting=1;r->phase=0;
 while(r->connecting){if(r->cancelled&&r->cancelled(r->context))return fail(r,A04_WLAN_STATE);if(r->now(r->context)>=deadline){TRACE(A04_CT_WAIT,A04_CP_BACKEND,A04_WLAN_TIMEOUT,r->phase);return fail(r,A04_WLAN_TIMEOUT);}if(a04_station_runtime_poll(r))return -1;r->wait(r->context,1000);}
 return r->connected?0:-1;
}
