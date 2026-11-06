/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "keyboard.h"
#include "host_control.h"
typedef struct {unsigned short csr;unsigned char fifo[16];unsigned used,writes;} MOCK;
static unsigned char r8(void *p,unsigned off){(void)p;(void)off;return 0;}
static unsigned short r16(void *p,unsigned off){assert(off==0x12);return ((MOCK*)p)->csr;}
static void w8(void *p,unsigned off,unsigned char v){MOCK*m=p;++m->writes;if(off==0x20){assert(m->used<sizeof(m->fifo));m->fifo[m->used++]=v;}}
static void w16(void *p,unsigned off,unsigned short v){MOCK*m=p;++m->writes;if(off==0x12)m->csr=v;}
/* Count report bits independently from HID short-item headers. No IDs in this
 * boot descriptor; verify output padding and the legacy input layout. */
static void descriptor(void)
{
 unsigned n,oldn,i=0,size=0,count=0,in=0,out=0,padding=0;const unsigned char*d=KbHidOutputDescriptor(&n),*old=KbHidDescriptor(&oldn);
 assert(n>oldn && !memcmp(d,old,oldn-1));
 while(i<n){unsigned tag=d[i++],len=tag&3u,value=0,j;if(len==3)len=4;assert(i+len<=n);for(j=0;j<len;++j)value|=(unsigned)d[i+j]<<(8*j);i+=len;
  switch(tag&0xfcu){case 0x74:size=value;break;case 0x94:count=value;break;case 0x84:assert(0);break;
  case 0x80:in+=size*count;break;case 0x90:out+=size*count;if(value&1)padding+=size*count;break;default:break;}
 }
 assert(in==64 && out==8 && padding==3);
}
int main(void)
{
 KB_ENUM k={0};KB_CONTROL c;unsigned char led,report[3]={0,0,0};unsigned i;MOCK m={0};
 MT8167_USB20_IO io={&m,r8,r16,w8,w16};struct a04_control_host host;struct a04_control_request r,*done;
 MT8167_USB20_EVENTS event={0};event.tx=1;
 k.stage=KB_READY;k.interface_number=7;k.max_packet0=8;descriptor();
 {unsigned char src[8]={1,2,3,4,5,6,7,8},dst[10];memset(dst,0xaa,sizeof(dst));
  assert(!KbCopyInputReport(dst,8,src) && !memcmp(dst,src,8) && dst[8]==0xaa);
  assert(!KbCopyInputReport(dst,9,src) && !dst[0] && !memcmp(dst+1,src,8) && dst[9]==0xaa);
  memcpy(dst,src,8);assert(!KbCopyInputReport(dst,9,dst) && !dst[0] && !memcmp(dst+1,src,8));
  memset(dst,0xaa,sizeof(dst));assert(KbCopyInputReport(dst,7,src)<0 && dst[0]==0xaa);
  assert(KbCopyInputReport(dst,10,src)<0 && dst[0]==0xaa);
  assert(KbCopyInputReport(0,8,src)<0 && KbCopyInputReport(dst,8,0)<0);
 }
 assert(!a04_control_init(&host,&io));assert(!a04_port_power(&host.port,1,1));assert(!a04_port_connect(&host.port,1));
 assert(!a04_port_reset_begin(&host.port));assert(!a04_port_reset_done(&host.port,0));assert(!a04_queue_online(&host.queue));
 for(i=0;i<32;++i){
  unsigned writes=m.writes;report[1]=(unsigned char)i;led=255;
  assert(!KbOutputControl(&k,report,2,0,&c,&led));assert(m.writes==writes && led==i);
  assert(c.length==1 && c.address==1 && c.max_packet0==8);
  led=255;assert(!KbOutputControl(&k,report+1,1,0,&c,&led) && led==i);
  assert(!memcmp(c.setup,(unsigned char[]){0x21,9,0,2,7,0,1,0},8));
  memset(&r,0,sizeof(r));memcpy(r.setup,c.setup,8);r.buffer=&led;r.address=c.address;r.speed=0x80;r.max_packet=c.max_packet0;m.used=0;
  assert(!a04_control_submit(&host,&r));assert(!a04_control_start(&host));assert(m.used==8);
  /* Setup completion sends exactly the one LED byte, without Windows ID. */
  m.csr=0;assert(!a04_control_events(&host,event,&done));assert(!done && m.used==9 && m.fifo[8]==i && m.csr==2);
  /* TX ready still asserted is not an acknowledged OUT completion. */
  assert(!a04_control_events(&host,event,&done) && !done);
  m.csr=0;assert(!a04_control_events(&host,event,&done));assert(!done && m.csr==0x60);
  assert(!a04_control_events(&host,event,&done) && !done);
  m.csr=0;assert(a04_control_events(&host,event,&done)==1 && done==&r);
  assert(r.transfer.actual==1 && r.transfer.result==A04_RESULT_OK);
 }
 /* Malformed reports never change the caller's packet or data. */
 memset(&c,0x5a,sizeof(c));{KB_CONTROL saved=c;led=0xaa;
 assert(KbOutputControl(&k,0,2,0,&c,&led)<0);
 assert(KbOutputControl(&k,report,0,0,&c,&led)<0);
 assert(KbOutputControl(&k,report,3,0,&c,&led)<0);
 assert(KbOutputControl(&k,report,2,1,&c,&led)<0);
 report[0]=1;assert(KbOutputControl(&k,report,2,0,&c,&led)<0);report[0]=0;
 report[1]=0x20;assert(KbOutputControl(&k,report,2,0,&c,&led)<0);report[1]=0;
 k.stage=KB_PROTOCOL;assert(KbOutputControl(&k,report,2,0,&c,&led)<0);k.stage=KB_READY;
 k.interface_number=256;assert(KbOutputControl(&k,report,2,0,&c,&led)<0);k.interface_number=7;
 k.max_packet0=0;assert(KbOutputControl(&k,report,2,0,&c,&led)<0);k.max_packet0=8;
 assert(KbOutputControl(0,report,2,0,&c,&led)<0);
 assert(KbOutputControl(&k,report,2,0,0,&led)<0);
 assert(KbOutputControl(&k,report,2,0,&c,0)<0);
 assert(!memcmp(&saved,&c,sizeof(c)) && led==0xaa);}
 /* STALL is a failure, and retains ownership until explicit quiescence. */
 m.used=0;assert(!a04_control_submit(&host,&r));assert(!a04_control_start(&host));m.csr=4;
 assert(!a04_control_events(&host,event,&done) && !done && r.transfer.state==A04_XFER_ABORTING);
 assert(!a04_control_quiesced(&host,&done) && done==&r && r.transfer.result==A04_RESULT_FAILED);
 return 0;
}
