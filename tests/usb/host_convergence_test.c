/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "board_host.h"
typedef struct {
 unsigned char phy[2048],mac[256];unsigned ms,role_reads,clear_after,source_on,source_off,power_on,power_off,writes;
 int stuck,session_drop,power_extra,admission,powered,vbus,session;
} M;
static int power(void*p,int on){M*m=p;if(on){++m->power_on;if(m->admission)return m->admission;}else ++m->power_off;m->powered=on;return 0;}
static int source(void*p,int on){M*m=p;if(on)++m->source_on;else ++m->source_off;m->vbus=on;return 0;}
static unsigned char pr(void*p,unsigned o){return ((M*)p)->phy[o];}
static void pw(void*p,unsigned o,unsigned char v){M*m=p;++m->writes;m->phy[o]=v;}
static unsigned pr32(void*p,unsigned o){(void)p;assert(o==0x70c);return 344;}
static unsigned char mr(void*p,unsigned o){M*m=p;unsigned char value=m->mac[o];
 if(o==0x60){value&=(unsigned char)~(0x80u|4u); /* No HOSTMODE until a device connects. */
  if(m->session){if(m->stuck || m->role_reads++<m->clear_after)value|=0x80;if(m->session_drop)value&=(unsigned char)~1u;}
 } else if(o==1)value|=(unsigned char)m->power_extra;
 return value;
}
static void mw(void*p,unsigned o,unsigned char v){M*m=p;++m->writes;m->mac[o]=v;if(o==0x60)m->session=!!(v&1u);}
static void delay(void*p,unsigned ms){((M*)p)->ms+=ms;}
static A04_USB_BOARD init(M*m){A04_USB_BOARD b={m,power,source,pr,pw,mr,mw,delay,pr32};memset(m,0,sizeof(*m));m->phy[0x710]=1;return b;}
int main(void)
{
 M m;A04_USB_BOARD b;int r;unsigned i,state;
 b=init(&m);assert(!A04UsbHostStart(&b) && m.ms==113 && !(m.mac[0x60]&4));
 assert(!A04UsbHostHealth(&b,&state));m.mac[1]|=8;assert(!A04UsbHostHealth(&b,&state)); /* reset must not false-fault */
 m.session_drop=1;assert(A04UsbHostHealth(&b,&state)<0);m.session_drop=0;m.stuck=1;assert(A04UsbHostHealth(&b,&state)<0);
 m.stuck=0;assert(!A04UsbHostHealth(&b,&state));A04UsbHostStop(&b);assert(A04UsbHostHealth(&b,&state)<0);
 /* Old code falsely succeeded here because POWER read back 0x21 even while
  * BDEVICE stayed asserted. No role/PHY writes are repeated during polling. */
 b=init(&m);m.stuck=1;assert(!A04UsbHostPrepare(&b));r=A04UsbHostRun(&b);
 assert(r==A04_USB_HOST_TRANSITION_TIMEOUT && m.ms==1113 && m.source_on==1 && m.vbus);
 A04UsbHostStop(&b);assert(!m.vbus&&!m.powered&&m.source_off==1&&m.power_off==1&&!(m.mac[0x60]&1));
 for(i=1;i<=200;++i){b=init(&m);m.clear_after=i;assert(!A04UsbHostStart(&b)&&m.ms==113+5*i&&m.source_on==1);A04UsbHostStop(&b);}
 b=init(&m);m.clear_after=201;assert(A04UsbHostStart(&b)<0&&!m.vbus&&!m.powered&&m.ms==1113);
 b=init(&m);m.session_drop=1;assert(A04UsbHostStart(&b)<0&&!m.vbus&&!m.powered&&m.ms==1113);
 /* RO HSMODE may be set. SUSPENDM/RESUME/RESET/SOFTCONN may not persist. */
 b=init(&m);m.power_extra=0x10;assert(!A04UsbHostStart(&b));A04UsbHostStop(&b);
 for(i=0;i<4;++i){b=init(&m);m.power_extra=(int[]){2,4,8,0x40}[i];assert(!A04UsbHostPrepare(&b));assert(A04UsbHostRun(&b)==A04_USB_HOST_POWER_STATE);A04UsbHostStop(&b);assert(!m.vbus&&!m.powered);}
 /* Device-role/external-source admission is handled before any MAC/PHY/source
  * operation. This fixture never changes the raw eligibility or cable role. */
 for(i=1;i<=2;++i){b=init(&m);m.admission=(int)i;assert(A04UsbHostStart(&b)<0&&m.power_on==1&&!m.writes&&!m.source_on&&!m.powered);}
 return 0;
}
