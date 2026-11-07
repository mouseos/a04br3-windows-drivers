#include <assert.h>
#include <string.h>
#include "../drivers/network/mt6630/station_runtime.c"
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
/* Exercise the real packet/descriptor/AP parsers, no synthesized scan-done. */
int main(void){
 struct a04_wmt_bridge b={0};struct a04_station_runtime r={0};uint8_t p[128]={0};
 r.bridge=&b;r.initialized=1;r.now=now;b.scan.pending=1;b.regulatory.permitted=1u<<6;
 unsigned n=16+36;uint8_t *f=p+16;p[2]=1;p[3]=0xe0;p[5]=6;f[0]=0x80;
 f[10]=f[16]=2;f[15]=f[21]=1;f[32]=100;f[34]=1;
 /* SSID x, one supported basic rate, DS channel6. */
 f[36]=0;f[37]=1;f[38]='x';f[39]=1;f[40]=1;f[41]=0x82;f[42]=3;f[43]=1;f[44]=6;n+=9;p[0]=n;
 f[44]=11;assert(packet(&r,p,n)==0&&!r.failed&&b.scan.pending&&!b.scan.complete&&!r.bss_count);
 assert(r.rejected_aps==1&&r.rejected_ap_error==A04_WLAN_FORMAT&&r.rejected_ap_bytes==n&&r.rejected_ap_descriptor==0xe001);
 f[44]=6;f[43]=2;assert(!packet(&r,p,n)&&r.rejected_aps==2&&!r.failed&&!r.bss_count);
 f[43]=1;assert(!packet(&r,p,n)&&r.bss_count==1&&!r.failed&&b.scan.pending&&!b.scan.complete);
 /* Transport/descriptor corruption remains fatal; no fabricated results. */
 p[0]=n-1;assert(packet(&r,p,n)==A04_WLAN_FORMAT&&r.failed==A04_WLAN_FORMAT&&r.rx_phase==2&&r.rx_error==A04_WLAN_FORMAT);
 assert(r.bss_count==1&&b.scan.pending&&!b.scan.complete);
 return 0;
}
