#include <assert.h>
#include "board_host.h"
typedef struct { unsigned char phy[256],mac[256]; unsigned power,vbus,ms; int fail; } Mock;
static int power(void *p,int x){Mock*m=p;m->power=x;return 0;}
static int vbus(void *p,int x){Mock*m=p;if(x&&m->fail){m->vbus=1;return -1;}m->vbus=x;return 0;}
static unsigned char pr(void*p,unsigned o){return ((Mock*)p)->phy[o];}
static void pw(void*p,unsigned o,unsigned char v){((Mock*)p)->phy[o]=v;}
static unsigned char mr(void*p,unsigned o){return ((Mock*)p)->mac[o];}
static void mw(void*p,unsigned o,unsigned char v){((Mock*)p)->mac[o]=v;}
static void delay(void*p,unsigned ms){((Mock*)p)->ms+=ms;}
int main(void){Mock m={0}; A04_USB_BOARD b={&m,power,vbus,pr,pw,mr,mw,delay};
 assert(!A04UsbHostStart(&b)); assert(m.power && m.vbus && m.ms==112);
 assert((m.mac[0x60]&1) && (m.mac[0x74]&0xc0)==0x80 && (m.phy[0x6c]&0x3c)==0x2c);
 A04UsbHostStop(&b); assert(!m.power && !m.vbus && !(m.mac[0x60]&1));
 m.fail=1; assert(A04UsbHostStart(&b)==-1 && !m.power && !m.vbus && !(m.mac[0x60]&1) && (m.phy[0x6c]&0x3e)==0x10); return 0; }
