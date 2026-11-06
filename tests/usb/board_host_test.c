#include <assert.h>
#include "board_host.h"
typedef struct { unsigned char phy[2048],mac[256]; unsigned power,vbus,ms; int fail; } Mock;
static int power(void *p,int x){Mock*m=p;m->power=x;return 0;}
static int vbus(void *p,int x){Mock*m=p;if(x&&m->fail){m->vbus=1;return -1;}m->vbus=x;return 0;}
static unsigned char pr(void*p,unsigned o){return ((Mock*)p)->phy[o];}
static void pw(void*p,unsigned o,unsigned char v){((Mock*)p)->phy[o]=v;}
static unsigned char mr(void*p,unsigned o){return ((Mock*)p)->mac[o];}
static void mw(void*p,unsigned o,unsigned char v){((Mock*)p)->mac[o]=v;}
static unsigned pr32(void*p,unsigned o){Mock*m=p;assert(o==0x70c);return m->phy[o]|((unsigned)m->phy[o+1]<<8);}
static void delay(void*p,unsigned ms){((Mock*)p)->ms+=ms;}
int main(void){Mock m={0}; A04_USB_BOARD b={&m,power,vbus,pr,pw,mr,mw,delay,pr32};
 m.phy[0x710]=1;m.phy[0x70c]=0x58;m.phy[0x70d]=1;
 assert(!A04UsbHostPrepare(&b)); assert(m.power && !m.vbus && !(m.mac[0x60]&1));
 /* The caller can arm CONNECT here before starting the session. */
 assert(!A04UsbHostRun(&b)); assert(m.vbus && (m.mac[0x60]&1) && m.mac[1]==0x21 && m.phy[0x15]==0x40 && !(m.phy[0x703]&1) && !m.phy[0x711]);
 A04UsbHostStop(&b); m.ms=0;
 assert(!A04UsbHostStart(&b)); assert(m.power && m.vbus && m.ms==113);
 assert((m.mac[0x60]&1) && (m.mac[0x74]&0xc0)==0x80 && (m.phy[0x6c]&0x3c)==0x2c);
 A04UsbHostStop(&b); assert(!m.power && !m.vbus && !(m.mac[0x60]&1));
 m.phy[0x710]=0;m.ms=0;assert(!A04UsbHostStart(&b) && m.phy[0x15]==0x40 && m.ms==3113);A04UsbHostStop(&b);
 m.phy[0x710]=1;m.phy[0x70c]=m.phy[0x70d]=0;
 assert(A04UsbHostPrepare(&b)<0 && !m.power && !m.vbus && !(m.phy[0x15]&0x80));
 m.phy[0x70c]=0x58;m.phy[0x70d]=1;
 m.fail=1; assert(A04UsbHostStart(&b)==-1 && !m.power && !m.vbus && !(m.mac[0x60]&1) && (m.phy[0x6c]&0x3e)==0x10); return 0; }
