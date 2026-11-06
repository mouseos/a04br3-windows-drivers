/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "wmt_bridge.h"
#include "native_tx.h"
static int alive(const struct a04_wmt_bridge *b)
{ return b && b->life && b->life->state==A04_WIFI_LIFE_IO && b->life->owner &&
 b->life->owner->active && !b->life->owner->poisoned &&
 b->life->generation==b->life->owner->generation; }
static int fault(struct a04_wmt_bridge *b,int e)
{
 b->failed=1;b->wmt_ready=b->wlan_ready=0;
 if(b->life && (b->life->state==A04_WIFI_LIFE_IO || b->life->state==A04_WIFI_LIFE_FAILED)){b->life->state=A04_WIFI_LIFE_FAILED;b->life->transport.ready=0;
  if(b->life->owner && !b->life->owner->poisoned)a04_wifi_fault(b->life->owner);}
 return e?e:-1;
}
static int command(void *context,unsigned op,uint32_t arg,unsigned response,uint32_t *out)
{
 struct a04_wmt_bridge *b=context;uint8_t value=(uint8_t)arg;int e;
 if(op!=52 || response!=1 || !out)return -1;
 e=a04_wifi_life_cmd52(b->life,arg>>31,(arg>>28)&7,(arg>>27)&1,(arg>>9)&0x1ffff,&value);
 if(e)return e;
 *out=value;return 0;
}
static void delay_us(void *context,unsigned us)
{struct a04_wmt_bridge *b=context;b->delay(b->delay_context,us);}
static int transfer(void *context,uint32_t arg,void *buffer,unsigned length)
{
 struct a04_wmt_bridge *b=context;
 return a04_wifi_life_cmd53(b->life,arg>>31,(arg>>28)&7,(arg>>26)&1,(arg>>9)&0x1ffff,buffer,length);
}
static int verify_block(struct a04_wmt_bridge *b,unsigned fn)
{
 uint8_t lo=0,hi=0;unsigned base=fn<<8;
 if(a04_wifi_life_cmd52(b->life,0,0,0,base+0x10,&lo) ||
    a04_wifi_life_cmd52(b->life,0,0,0,base+0x11,&hi) || lo || hi!=2)return -1;
 return a04_wifi_life_block_verified(b->life,fn,512);
}
int a04_wmt_bridge_open(struct a04_wmt_bridge *b,struct a04_wifi_life *life,unsigned functions,void *context,void (*delay)(void *,unsigned))
{
 uint32_t hw,rom;int e;struct a04_sdio_wmt_state state={0};
 if(!b||!life||!delay||functions<2||functions>7||life->state!=A04_WIFI_LIFE_IO || b->identity_ready || b->failed)return -1;
 b->life=life;b->delay_context=context;b->delay=delay;b->io.ctx=b;b->io.command=command;b->io.delay=delay_us;
 b->transport.io=&b->io;b->transport.transfer=transfer;b->transport.functions=functions;b->transport.enabled=0;b->transport.failed=0;
 e=a04_sdio_wmt_prepare(&b->transport,&state);if(e)return fault(b,e);
 if(verify_block(b,2))return fault(b,-1);
 e=a04_wmt_channel_init(&b->transport,&b->channel);if(e)return fault(b,e);
 e=a04_wmt_read_version(&b->transport,&b->channel,0x80000000u,&hw);if(e)return fault(b,e);
 e=a04_wmt_read_version(&b->transport,&b->channel,0x80000004u,&rom);if(e)return fault(b,e);
 b->identity.chip=0x6630;b->identity.hardware=hw&0xffffu;b->identity.rom=rom&0xffffu;b->identity_ready=1;return 0;
}
int a04_wmt_bridge_boot_wmt(struct a04_wmt_bridge *b,const void *data,unsigned bytes,
 const uint8_t expected[32],int (*sha256)(const uint8_t *,size_t,uint8_t[32]),
 const struct a04_wmt_startup_options *options)
{
 uint8_t digest[32];unsigned different=0,i;int e;
 if(!alive(b)||!b->identity_ready||b->failed||!data||!bytes||bytes>A04_WMT_FW_MAX_BUNDLE||!expected||!sha256||!options||b->wmt_ready)return -1;
 if(sha256(data,bytes,digest))return -1;
 for(i=0;i<32;i++)different|=digest[i]^expected[i];
 if(different)return -1;
 e=a04_wmt_firmware_bundle(&b->identity,data,bytes,&b->wmt_fw);if(e)return e;
 e=a04_wmt_download_run(&b->transport,&b->channel,&b->wmt_fw,&b->identity,options,&b->wmt_state,0,0);
 b->wmt_fw.count=0; /* discard borrowed views after synchronous transfer */
 if(e)return fault(b,e);
 b->wmt_ready=b->wmt_state.stp_ready;return b->wmt_ready?0:fault(b,-1);
}
int a04_wmt_bridge_boot_wlan(struct a04_wmt_bridge *b,const void *data,unsigned bytes,int (*sha256)(const uint8_t *,size_t,uint8_t[32]))
{
 uint8_t digest[32];int e;
 if(!alive(b)||!b->wmt_ready||!b->wmt_state.function1_ready||b->failed||!data||bytes!=237008u||!sha256||b->wlan_ready)return -1;
 if(sha256(data,bytes,digest))return -1;
 e=a04_wlan_firmware_select(&b->identity,data,bytes,digest,&b->wlan_fw);if(e)return e;
 e=a04_wlan_host_open(&b->wlan,&b->transport);if(e)return fault(b,e);
 if(verify_block(b,1))return fault(b,-1);
 e=a04_wlan_download(&b->wlan,&b->wlan_fw,&b->wlan_state);b->wlan_fw.count=0;if(e)return fault(b,e);
 b->wlan_ready=b->wlan_state.ready;return b->wlan_ready?0:fault(b,-1);
}

static int manufacture_set(void *context,unsigned cid,const uint8_t *data,unsigned length)
{
 struct a04_wmt_bridge *b=context;int e=a04_wlan_set(&b->wlan,cid,data,length,NULL);
 volatile uint8_t *p=b->wlan.tx+8;for(unsigned i=0;i<length;i++)p[i]=0;
 return e;
}
int a04_wmt_bridge_load_calibration(struct a04_wmt_bridge *b,const uint8_t data[512])
{
 struct a04_regulatory r;int e;
 if(!alive(b)||!b->wlan_ready||b->failed||!data||b->calibration_submitted||
    b->scan.pending||b->pending_wlan_status||a04_regulatory_decode(data,&r))return -1;
 if(!b->wlan.runtime){e=a04_wlan_runtime_begin(&b->wlan);if(e)return fault(b,e);}
 e=a04_manufacture_submit(data,&r,b,manufacture_set);if(e)return fault(b,e);
 b->regulatory=r;b->calibration_submitted=1;return 0;
}

int a04_wmt_bridge_scan_begin(struct a04_wmt_bridge *b,const struct a04_wlan_scan *request)
{
 int e;if(!alive(b)||!b->wlan_ready||b->failed||!b->wlan.runtime||!request)return -1;
 if(b->calibration_submitted){
  unsigned mask=request->active?b->regulatory.active:b->regulatory.permitted;
  if(request->channel_type!=4||!request->channels||!request->channel_count)return -1;
  for(unsigned i=0;i<request->channel_count;i++){unsigned ch=request->channels[2*i+1];if(request->channels[2*i]!=1||ch>13||!ch||!(mask&(1u<<ch)))return -1;}
 }
 e=a04_wlan_scan_begin(&b->wlan,&b->scan,request);
 if(e && (b->scan.failed||b->wlan.failed||b->transport.failed))return fault(b,e);
 return e;
}
int a04_wmt_bridge_scan_event(struct a04_wmt_bridge *b,const uint8_t *packet,unsigned length)
{
 struct a04_wlan_event event;int e;
 if(!alive(b)||!b->wlan_ready||b->failed||!b->scan.pending||!packet)return -1;
 e=a04_wlan_event_parse(packet,length,&event);if(e)return fault(b,e);
 e=a04_wlan_scan_done(&b->scan,&event);
 if(e && e!=A04_WLAN_UNSUPPORTED)return fault(b,e);
 return e;
}
int a04_wmt_bridge_scan_consume(struct a04_wmt_bridge *b)
{
 if(!alive(b)||!b->scan.complete||b->scan.failed||b->scan.pending)return -1;
 b->scan.complete=0;return 0;
}
int a04_wmt_bridge_scan_fail(struct a04_wmt_bridge *b,int reason)
{
 if(!b||!b->life||!reason||!b->scan.pending||
    (b->life->state!=A04_WIFI_LIFE_IO && b->life->state!=A04_WIFI_LIFE_FAILED))return -1;
 b->scan.pending=0;b->scan.failed=reason;fault(b,reason);return 0;
}
static int bridge_runtime(const struct a04_wmt_bridge *b)
{ return alive(b) && b->wlan_ready && !b->failed && b->wlan.runtime; }
static int bridge_error(struct a04_wmt_bridge *b,int e)
{ if(e && (b->wlan.failed || b->transport.failed))return fault(b,e);return e; }
int a04_wmt_bridge_station_setup(struct a04_wmt_bridge *b,struct a04_wlan_station *s,const struct a04_wlan_local *l)
{ if(!bridge_runtime(b))return -1;return bridge_error(b,a04_wlan_station_setup(s,&b->wlan,l)); }
int a04_wmt_bridge_station_select(struct a04_wmt_bridge *b,struct a04_wlan_station *s,const struct a04_wlan_ap *ap,unsigned secured,unsigned token,unsigned interval,uint64_t now,uint64_t deadline)
{ if(!bridge_runtime(b))return -1;return bridge_error(b,a04_wlan_station_select(s,&b->wlan,ap,secured,token,interval,now,deadline)); }
int a04_wmt_bridge_station_event(struct a04_wmt_bridge *b,struct a04_wlan_station *s,const uint8_t *p,unsigned n,uint64_t now)
{ struct a04_wlan_event event;int e;if(!bridge_runtime(b))return -1;e=a04_wlan_event_parse(p,n,&event);if(e)return fault(b,e);return bridge_error(b,a04_wlan_station_event(s,&b->wlan,&event,now)); }
int a04_wmt_bridge_station_auth(struct a04_wmt_bridge *b,struct a04_wlan_station *s,uint64_t now)
{ if(!bridge_runtime(b))return -1;return bridge_error(b,a04_wlan_station_auth(s,&b->wlan,now)); }
int a04_wmt_bridge_station_auth_rx(struct a04_wmt_bridge *b,struct a04_wlan_station *s,const uint8_t *p,unsigned n,uint64_t now)
{ if(!bridge_runtime(b))return -1;return bridge_error(b,a04_wlan_station_auth_rx(s,&b->wlan,p,n,now)); }
int a04_wmt_bridge_station_assoc(struct a04_wmt_bridge *b,struct a04_wlan_station *s,unsigned capability,unsigned listen,const uint8_t *ie,unsigned length,uint64_t now)
{ if(!bridge_runtime(b))return -1;return bridge_error(b,a04_wlan_station_assoc(s,&b->wlan,capability,listen,ie,length,now)); }
int a04_wmt_bridge_station_assoc_rx(struct a04_wmt_bridge *b,struct a04_wlan_station *s,const uint8_t *p,unsigned n,unsigned secured,uint64_t now)
{ if(!bridge_runtime(b))return -1;return bridge_error(b,a04_wlan_station_assoc_rx(s,&b->wlan,p,n,secured,now)); }
int a04_wmt_bridge_security_begin(struct a04_wmt_bridge *b,struct a04_wlan_security *sec,const struct a04_wlan_station *s,uint64_t now,uint64_t deadline)
{ if(!bridge_runtime(b))return -1;return a04_wlan_security_begin(sec,s,now,deadline); }
int a04_wmt_bridge_security_rx(struct a04_wmt_bridge *b,struct a04_wlan_security *sec,const uint8_t *p,unsigned n,a04_wlan_mic_verify verify,void *ctx,uint64_t now)
{ if(!bridge_runtime(b))return -1;return a04_wlan_security_rx(sec,p,n,verify,ctx,now); }
int a04_wmt_bridge_key_install(struct a04_wmt_bridge *b,struct a04_wlan_security *sec,struct a04_wlan_station *s,unsigned pairwise,unsigned id,const uint8_t key[16],const uint8_t rsc[8],uint64_t now)
{ if(!bridge_runtime(b))return -1;return bridge_error(b,a04_wlan_security_install(sec,s,&b->wlan,pairwise,id,key,rsc,now)); }
int a04_wmt_bridge_key_event(struct a04_wmt_bridge *b,struct a04_wlan_security *sec,const struct a04_wlan_station *s,const uint8_t *p,unsigned n,uint64_t now)
{ struct a04_wlan_event event;int e;if(!bridge_runtime(b))return -1;e=a04_wlan_event_parse(p,n,&event);if(e)return fault(b,e);return a04_wlan_security_key_event(sec,s,&event,now); }
int a04_wmt_bridge_authorize(struct a04_wmt_bridge *b,struct a04_wlan_security *sec,int result,uint64_t now)
{ if(!bridge_runtime(b))return -1;return a04_wlan_security_authorize(sec,result,now); }
int a04_wmt_bridge_tx_ethernet(struct a04_wmt_bridge *b,const struct a04_wlan_station *station,
 struct a04_wlan_security *security,struct a04_wlan_data_state *data,const uint8_t *frame,unsigned length,
 a04_wlan_mic_verify verify,void *context,uint64_t now,uint64_t deadline)
{
 int e;if(!alive(b)||!b->wlan_ready||b->failed||!b->wlan.runtime)return -1;
 e=a04_wlan_ethernet_send(&b->wlan,station,security,data,frame,length,verify,context,now,deadline);
 if(e && (b->wlan.failed||b->transport.failed))return fault(b,e);
 return e;
}
int a04_wmt_bridge_tx_mpdu(struct a04_wmt_bridge *b,const struct a04_wlan_station *station,
 struct a04_wlan_security *security,struct a04_wlan_data_state *data,const uint8_t *mpdu,unsigned length,
 uint8_t *scratch,size_t capacity,a04_wlan_mic_verify verify,void *context,uint64_t now,uint64_t deadline)
{
 size_t bytes;
 if(!alive(b)||!station||!mpdu||!scratch||a04_native_tx_to_ethernet(mpdu,length,station->ap.mac,station->local.mac,scratch,capacity,&bytes))return -1;
 return a04_wmt_bridge_tx_ethernet(b,station,security,data,scratch,(unsigned)bytes,verify,context,now,deadline);
}
int a04_wmt_bridge_rx_ethernet(struct a04_wmt_bridge *b,const struct a04_wlan_station *station,
 struct a04_wlan_security *security,const uint8_t *packet,unsigned length,struct a04_wlan_ethernet *out)
{
 if(!alive(b)||!b->wlan_ready||b->failed||!b->wlan.runtime)return -1;
 return a04_wlan_ethernet_rx(station,security,packet,length,out);
}
int a04_wmt_bridge_tx_event(struct a04_wmt_bridge *b,const struct a04_wlan_station *station,
 struct a04_wlan_security *security,struct a04_wlan_data_state *data,const uint8_t *packet,unsigned length,uint64_t now)
{
 struct a04_wlan_event event;int e;if(!alive(b)||!b->wlan_ready||b->failed||!b->wlan.runtime)return -1;
 e=a04_wlan_event_parse(packet,length,&event);if(e)return fault(b,e);
 e=a04_wlan_data_tx_event(&b->wlan,station,security,data,&event,now);
 if(e && (b->wlan.failed||b->transport.failed))return fault(b,e);
 return e;
}
int a04_wmt_bridge_poll_wlan_status(struct a04_wmt_bridge *b,uint32_t *status)
{
 uint32_t observed;int e;
 if(!alive(b)||!b->wlan_ready||b->failed||!status||b->pending_wlan_status)return -1;
 e=a04_wlan_interrupt_status(&b->wlan,&observed);if(e)return fault(b,e);
 b->pending_wlan_status=observed;*status=observed;return 0;
}
int a04_wmt_bridge_receive(struct a04_wmt_bridge *b,unsigned port,unsigned polls,
 const uint8_t **data,unsigned *length)
{
 int e;if(!alive(b)||!b->wlan_ready||b->failed||!b->pending_wlan_status||!data||!length||port>1||!polls||polls>20000)return -1;
 e=a04_wlan_receive(&b->wlan,port,polls);if(e)return fault(b,e);
 *data=b->wlan.rx;*length=b->wlan.rx_length;return 0;
}
int a04_wmt_bridge_ack_wlan_status(struct a04_wmt_bridge *b,uint32_t handled)
{
 if(!alive(b) || !b->wlan_ready || b->failed || !b->pending_wlan_status || handled!=b->pending_wlan_status)return -1;
 b->pending_wlan_status=0;return 0;
}
