#include <assert.h>
#include <string.h>
#include "../drivers/network/mt6630/station_runtime.h"
static unsigned status_value,acks,polls;
static uint64_t now(void *p){(void)p;return 1;}
int a04_wmt_bridge_poll_wlan_status(struct a04_wmt_bridge *b,uint32_t *status){polls++;*status=status_value;b->pending_wlan_status=status_value;b->wlan.status=status_value;return 0;}
int a04_wmt_bridge_ack_wlan_status(struct a04_wmt_bridge *b,uint32_t handled){assert(handled==status_value&&b->pending_wlan_status==handled);acks++;b->pending_wlan_status=0;return 0;}
int a04_wmt_bridge_receive(struct a04_wmt_bridge * p0,unsigned p1,unsigned p2,const uint8_t **p3,unsigned *p4){assert(!"unexpected non-notification operation");return -1;}
int a04_wmt_bridge_scan_event(struct a04_wmt_bridge * p0,const uint8_t * p1,unsigned p2){assert(!"unexpected non-notification operation");return -1;}
int a04_wmt_bridge_station_assoc(struct a04_wmt_bridge * p0,struct a04_wlan_station * p1,unsigned p2,unsigned p3,const uint8_t * p4,unsigned p5,uint64_t p6){assert(!"unexpected non-notification operation");return -1;}
int a04_wmt_bridge_station_assoc_rx(struct a04_wmt_bridge * p0,struct a04_wlan_station * p1,const uint8_t * p2,unsigned p3,unsigned p4,uint64_t p5){assert(!"unexpected non-notification operation");return -1;}
int a04_wmt_bridge_station_auth(struct a04_wmt_bridge * p0,struct a04_wlan_station * p1,uint64_t p2){assert(!"unexpected non-notification operation");return -1;}
int a04_wmt_bridge_station_auth_rx(struct a04_wmt_bridge * p0,struct a04_wlan_station * p1,const uint8_t * p2,unsigned p3,uint64_t p4){assert(!"unexpected non-notification operation");return -1;}
int a04_wmt_bridge_station_event(struct a04_wmt_bridge * p0,struct a04_wlan_station * p1,const uint8_t * p2,unsigned p3,uint64_t p4){assert(!"unexpected non-notification operation");return -1;}
int a04_wmt_bridge_station_select(struct a04_wmt_bridge * p0,struct a04_wlan_station * p1,const struct a04_wlan_ap * p2,unsigned p3,unsigned p4,unsigned p5,uint64_t p6,uint64_t p7){assert(!"unexpected non-notification operation");return -1;}
int a04_wmt_bridge_station_setup(struct a04_wmt_bridge * p0,struct a04_wlan_station * p1,const struct a04_wlan_local * p2){assert(!"unexpected non-notification operation");return -1;}
int main(void){
 struct a04_wmt_bridge b;struct a04_station_runtime r;
 memset(&b,0,sizeof(b));memset(&r,0,sizeof(r));r.bridge=&b;r.initialized=1;r.now=now;
 b.wlan.cpu_used=7;b.wlan.tc0_used=3;b.wlan.tc1_used=9;b.wlan.ffa=2;b.wlan.fw_owned=0;
 status_value=0x80;assert(!a04_station_runtime_poll(&r));
 assert(polls==1&&acks==1&&!r.failed&&!b.pending_wlan_status);
 assert(b.wlan.cpu_used==7&&b.wlan.tc0_used==3&&b.wlan.tc1_used==9&&b.wlan.ffa==2&&!b.wlan.fw_owned);
 assert(!a04_station_runtime_poll(&r)&&polls==2&&acks==2&&!r.failed);
 for(unsigned i=0;i<3;i++){
  unsigned invalid[3]={0x08,0x40,0x80000000};r.failed=0;status_value=invalid[i]|0x80;unsigned before=acks;
  assert(a04_station_runtime_poll(&r)==A04_WLAN_STATUS&&r.failed==A04_WLAN_STATUS&&acks==before&&b.pending_wlan_status==status_value);
 }
 return 0;
}
