/* Production association encoder and fake HIF submission; no AP/RF success. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../drivers/network/mt6630/assoc_capability.h"
#include "../drivers/network/mt6630/vendor_xnu/A04BR3WLANSecurity.h"
static int send(void*c,uint32_t arg,void*p,unsigned n){(void)c;(void)p;assert((arg&0x80000000u)&&n>56);return 0;}
int main(void){
 unsigned cap=0;struct a04_wlan_local l={0};struct a04_wlan_station s={0};struct a04_wlan_host h={0};struct a04_sdio_transport tr={0};struct a04_sdio_io io={0};
 l.bss=l.own=1;l.sta=2;l.wlan=3;l.bmc=19;l.permitted_channels=l.active_channels=1u<<6;l.mac[0]=2;l.mac[5]=1;
 s.local=l;s.ap.mac[0]=2;s.ap.mac[5]=2;s.ap.channel=6;s.ap.band=1;s.ap.beacon=100;s.ap.ssid[0]='x';s.ap.ssid_length=1;s.ap.rates[0]=0x82;s.ap.rate_count=1;s.deadline_us=10000;
 /* Try every possible AP capability: accepted ESS values may advertise only
  * the actual station tuple; unsafe/contradictory security stays rejected. */
 for(unsigned raw=0;raw<65536;raw++){
  int ok=!a04_mt6630_assoc_capability(raw,!!(raw&16),&cap);
  assert(ok==!!((raw&1)&&!(raw&2)));if(ok)assert(!(cap&~0x411u)&&(cap&1)&&!!(cap&16)==!!(raw&16));
 }
 assert(a04_mt6630_assoc_capability(0x401,1,&cap));assert(a04_mt6630_assoc_capability(0x411,0,&cap));
 tr.io=&io;tr.transfer=send;tr.enabled=tr.functions=2;h.transport=&tr;h.initialized=h.runtime=h.ready=1;h.cpu_free=26;
 for(unsigned secured=0;secured<2;secured++){
  unsigned len=0;const uint8_t*rsn=secured?a04_wlan_rsn_selected(&len):NULL;
  s.secured=secured;s.join.phase=2;s.join.failed=s.failed=0;memcpy(s.join.ap,s.ap.mac,6);memcpy(s.join.client,l.mac,6);
  unsigned ap=0xffedu|(secured?16:0);assert(!a04_mt6630_assoc_capability(ap,secured,&cap));
  assert(!a04_wlan_station_assoc(&s,&h,cap,10,rsn,len,1));
  assert(h.tx[28]==0&&a04_wmt_le16(h.tx+52)==(secured?0x411:0x401));
  unsigned rsn_count=0;for(unsigned i=56;i<a04_wmt_le16(h.tx);){assert(i+2<=a04_wmt_le16(h.tx));if(h.tx[i]==48)rsn_count++;i+=h.tx[i+1]+2;}
  assert(rsn_count==secured);assert(s.join.phase==3); /* submitted, not associated */
 }
 puts("Station-only capability -> actual association frame, privacy/RSN and unsupported bit exclusion PASS; RF untested");
}
