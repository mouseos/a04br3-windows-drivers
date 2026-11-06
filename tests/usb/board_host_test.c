#include <assert.h>
#include "board_host.h"
typedef struct { unsigned char phy[256],mac[256]; unsigned power,vbus,ms,writes,on_calls,off_calls; int fail,source_bad,sense_bad; } Mock;
static int power(void *p,int x){Mock*m=p;m->power=x;return 0;}
static int vbus(void *p,int x){Mock*m=p;if(x)m->on_calls++;else m->off_calls++;if(x&&m->fail){m->vbus=1;return -1;}m->vbus=x;return 0;}
static int source(void *p){return ((Mock*)p)->source_bad;}
static int sense(void *p){return ((Mock*)p)->sense_bad;}
static unsigned char pr(void*p,unsigned o){return ((Mock*)p)->phy[o];}
static void pw(void*p,unsigned o,unsigned char v){Mock*m=p;m->writes++;m->phy[o]=v;}
static unsigned char mr(void*p,unsigned o){return ((Mock*)p)->mac[o];}
static void mw(void*p,unsigned o,unsigned char v){Mock*m=p;m->writes++;m->mac[o]=v;}
static void delay(void*p,unsigned ms){((Mock*)p)->ms+=ms;}
int main(void){Mock m={0}; A04_USB_BOARD b={&m,power,vbus,pr,pw,mr,mw,delay,source,sense};
 b.source_ready=0;assert(A04UsbHostPrepare(&b) && !m.power && !m.writes && !m.on_calls);b.source_ready=source;
 b.vbus_verified=0;assert(A04UsbHostPrepare(&b) && !m.power && !m.writes && !m.on_calls);b.vbus_verified=sense;
 m.source_bad=1;assert(A04UsbHostPrepare(&b) && !m.power && !m.writes && !m.on_calls);m.source_bad=0;
 assert(!A04UsbHostPrepare(&b)); assert(m.power && !m.vbus && !(m.mac[0x60]&1));
 assert(!A04UsbHostRun(&b)); assert(m.vbus && (m.mac[0x60]&1));
 A04UsbHostStop(&b); m.ms=0;
 assert(!A04UsbHostStart(&b)); assert(m.power && m.vbus && m.ms==112);
 assert((m.mac[0x60]&1) && (m.mac[0x74]&0xc0)==0x80 && (m.phy[0x6c]&0x3c)==0x2c);
 A04UsbHostStop(&b); assert(!m.power && !m.vbus && !(m.mac[0x60]&1));
 m.fail=1; assert(A04UsbHostStart(&b)==-1 && !m.power && !m.vbus && !(m.mac[0x60]&1));m.fail=0;
 m.sense_bad=1;assert(A04UsbHostStart(&b)==-1 && !m.power && !m.vbus && !(m.mac[0x60]&1));assert(m.off_calls>=3);
 return 0; }
