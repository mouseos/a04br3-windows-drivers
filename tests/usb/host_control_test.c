/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "host_control.h"
typedef struct { unsigned char reg[0x500], fifo[256], input[64];unsigned out,in; } MOCK;
static unsigned char r8(void *p,unsigned a){MOCK*m=p;if(a==0x20)return m->input[m->in++];return m->reg[a];}
static unsigned short r16(void *p,unsigned a){MOCK*m=p;return (unsigned short)(m->reg[a]|m->reg[a+1]<<8);}
static unsigned r32(void *p,unsigned a){MOCK*m=p;return (unsigned)m->reg[a]|((unsigned)m->reg[a+1]<<8)|((unsigned)m->reg[a+2]<<16)|((unsigned)m->reg[a+3]<<24);}
static void w8(void *p,unsigned a,unsigned char v){MOCK*m=p;if(a==0x20)m->fifo[m->out++]=v;else if(a==0x0a)m->reg[a]&=(unsigned char)~v;else m->reg[a]=v;}
static void w16(void *p,unsigned a,unsigned short v){MOCK*m=p;if(a==2||a==4){m->reg[a]&=(unsigned char)~v;m->reg[a+1]&=(unsigned char)~(v>>8);}else {m->reg[a]=(unsigned char)v;m->reg[a+1]=(unsigned char)(v>>8);}}
static void w32(void *p,unsigned a,unsigned v){MOCK*m=p;unsigned i;for(i=0;i<4;i++)m->reg[a+i]=(unsigned char)(v>>(8*i));}
static void irq(MOCK*m,MT8167_USB20_CONTROLLER*c,struct a04_control_host*h,unsigned usb,unsigned tx,struct a04_control_request **done,int expected)
{ MT8167_USB20_EVENTS e;m->reg[0xa0]=(unsigned char)((usb?4:0)|(tx?1:0));m->reg[0x0a]=(unsigned char)usb;m->reg[2]=(unsigned char)tx;assert(Mt8167UsbInterruptCapture(c,&e)==1);assert(a04_control_events(h,e,done)==expected); }
int main(void)
{
 MOCK m;MT8167_USB20_CONTROLLER c;struct a04_control_host h;struct a04_control_request a,b,*done;unsigned char data[8];
 memset(&m,0,sizeof(m));memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
 c.io=(MT8167_USB20_IO){&m,r8,r16,w8,w16};c.read32=r32;c.write32=w32;
 assert(!a04_control_init(&h,&c.io));assert(!a04_port_power(&h.port,1,1));assert(!a04_queue_online(&h.queue));assert(!Mt8167UsbInterruptEnable(&c));
 /* Boot-keyboard HID GET_REPORT(Input, id 0) on interface 3. */
 a.setup[0]=0xa1;a.setup[1]=1;a.setup[3]=1;a.setup[4]=3;a.setup[6]=8;a.buffer=data;a.speed=0x80;a.max_packet=8;
 assert(!a04_control_submit(&h,&a));assert(a04_control_start(&h)<0);
 irq(&m,&c,&h,0x10,0,&done,0);assert(!a04_port_reset_begin(&h.port));assert(!a04_port_reset_done(&h.port,0));
 assert(!a04_control_start(&h));assert(m.out==8 && !memcmp(m.fifo,a.setup,8));
 m.reg[0x12]=0;irq(&m,&c,&h,0,1,&done,0);assert(m.reg[0x12]==0x20);
 m.reg[0x12]=1;m.reg[0x18]=8;memset(m.input,0x42,8);irq(&m,&c,&h,0,1,&done,0);assert(data[7]==0x42 && m.reg[0x12]==0x42);
 m.reg[0x12]=0;irq(&m,&c,&h,0,1,&done,1);assert(done==&a && a.transfer.result==A04_RESULT_OK && a.transfer.actual==8);
 b=a;b.address=2;assert(!a04_control_submit(&h,&b));assert(!a04_control_start(&h));
 assert(a04_control_cancel(&h,&b)==1);m.reg[0x12]=0;irq(&m,&c,&h,0,1,&done,0);assert(!done && h.active==&b);
 /* Caller proves IRQ drain and controller flush before this handoff. */
 assert(!a04_control_quiesced(&h,&done) && done==&b && b.transfer.result==A04_RESULT_CANCELLED);
 assert(!a04_control_submit(&h,&b));assert(!a04_control_start(&h));
 m.reg[0x12]=4;irq(&m,&c,&h,0,1,&done,0);assert(b.transfer.state==A04_XFER_ABORTING && !done);
 assert(!a04_control_quiesced(&h,&done) && done==&b && b.transfer.result==A04_RESULT_FAILED);
 assert(!a04_control_submit(&h,&a));assert(!a04_control_start(&h));
 m.reg[0x12]=0;irq(&m,&c,&h,0x20,1,&done,0);assert(!done && a.transfer.state==A04_XFER_ABORTING);
 assert(!a04_control_quiesced(&h,&done) && done==&a && a.transfer.result==A04_RESULT_DISCONNECTED);
 assert(a04_control_start(&h)<0);
 assert(!a04_queue_online(&h.queue));assert(!a04_port_power(&h.port,1,1));
 irq(&m,&c,&h,0x10,0,&done,0);assert(!a04_port_reset_begin(&h.port));assert(!a04_port_reset_done(&h.port,0));
 assert(!a04_control_submit(&h,&a));assert(!a04_control_start(&h));
 irq(&m,&c,&h,0x80,1,&done,0);assert(!(h.port.status&A04_PORT_POWER) && !(h.port.status&A04_PORT_OVERCURRENT));
 assert(!a04_control_quiesced(&h,&done) && done==&a && a.transfer.result==A04_RESULT_DISCONNECTED);
 return 0;
}
