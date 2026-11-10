/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "ep0_pio.h"
struct mock {unsigned short csr;unsigned char fifo[128];unsigned reads,writes,control_writes,count;};
static unsigned char r8(void *p,unsigned r){struct mock*m=p;if(r==0x18)return (unsigned char)m->count;assert(r==0x20);return m->fifo[m->reads++];}
static unsigned short r16(void*p,unsigned r){assert(r==0x12);return ((struct mock*)p)->csr;}
static void w8(void*p,unsigned r,unsigned char v){struct mock*m=p;if(r==0xe){assert(!v);return;}assert(r==0x20);m->fifo[m->writes++]=v;}
static void w16(void*p,unsigned r,unsigned short v){struct mock*m=p;assert(r==0x12);m->csr=v;m->control_writes++;}
static void pending(MT8167_USB20_EP0*e,struct mock*m,unsigned csr,unsigned token){EP0_TRANSFER before=e->transfer;unsigned reads=m->reads,writes=m->writes;m->csr=(unsigned short)csr;assert(Mt8167Ep0Interrupt(e)==EP0_WAIT);assert(!memcmp(&before,&e->transfer,sizeof(before)));assert(m->reads==reads&&m->writes==writes&&m->csr==token&&e->fault==MT8167_EP0_FAULT_NONE);}
int main(void){struct mock m={0};MT8167_USB20_EP0 e;MT8167_USB20_IO io={&m,r8,r16,w8,w16};unsigned char in[8]={0x80,6,0,1,0,0,10,0},out[8]={0,9,0,0,0,0,10,0},data[10]={0};unsigned i;
 assert(Mt8167Ep0Start(&e,&io,in,data,10,8)==EP0_SEND_SETUP);m.csr=0;assert(Mt8167Ep0Interrupt(&e)==EP0_REQUEST_IN);
 for(i=0;i<1000;i++)pending(&e,&m,0xa0,0x20);
 m.count=8;m.csr=1;assert(Mt8167Ep0Interrupt(&e)==EP0_REQUEST_IN&&e.transfer.actual==8);pending(&e,&m,0x80,0x20);
 m.count=2;m.csr=1;assert(Mt8167Ep0Interrupt(&e)==EP0_STATUS_OUT&&e.transfer.actual==10);pending(&e,&m,0xc2,0x42);m.csr=0;assert(Mt8167Ep0Interrupt(&e)==EP0_COMPLETE);
 memset(&m,0,sizeof(m));assert(Mt8167Ep0Start(&e,&io,out,data,10,8)==EP0_SEND_SETUP);m.csr=0;assert(Mt8167Ep0Interrupt(&e)==EP0_SEND_OUT&&m.writes==16);
 for(i=0;i<1000;i++)pending(&e,&m,0x82,2);
 m.csr=0;assert(Mt8167Ep0Interrupt(&e)==EP0_SEND_OUT&&m.writes==18&&e.transfer.actual==8);pending(&e,&m,0x82,2);
 m.csr=0;assert(Mt8167Ep0Interrupt(&e)==EP0_STATUS_IN);pending(&e,&m,0xe0,0x60);m.csr=0;assert(Mt8167Ep0Interrupt(&e)==EP0_COMPLETE);
 {unsigned char no_data[8]={0,9,0,0,0,0,0,0};assert(Mt8167Ep0Start(&e,&io,no_data,0,0,8)==EP0_SEND_SETUP);m.csr=0;assert(Mt8167Ep0Interrupt(&e)==EP0_STATUS_IN);pending(&e,&m,0x80,0x60);m.csr=0;assert(Mt8167Ep0Interrupt(&e)==EP0_COMPLETE);}
 assert(Mt8167Ep0Start(&e,&io,in,data,10,8)==EP0_SEND_SETUP);m.csr=0;assert(Mt8167Ep0Interrupt(&e)==EP0_REQUEST_IN);m.csr=0x84;assert(Mt8167Ep0Interrupt(&e)==EP0_ABORT&&e.fault==MT8167_EP0_FAULT_STALL);
 assert(Mt8167Ep0Start(&e,&io,in,data,10,8)==EP0_SEND_SETUP&&e.fault==0);m.csr=0;assert(Mt8167Ep0Interrupt(&e)==EP0_REQUEST_IN);m.csr=0x90;assert(Mt8167Ep0Interrupt(&e)==EP0_ABORT&&e.fault==MT8167_EP0_FAULT_PROTOCOL);
 assert(Mt8167Ep0Start(&e,&io,in,data,10,8)==EP0_SEND_SETUP);m.csr=0x80;assert(Mt8167Ep0Interrupt(&e)==EP0_ABORT&&e.fault==MT8167_EP0_FAULT_PROTOCOL);
 assert(Mt8167Ep0Start(&e,&io,in,data,10,8)==EP0_SEND_SETUP);m.csr=0;assert(Mt8167Ep0Interrupt(&e)==EP0_REQUEST_IN);m.csr=0x81;assert(Mt8167Ep0Interrupt(&e)==EP0_ABORT&&e.fault==MT8167_EP0_FAULT_PROTOCOL);
 return 0;}
