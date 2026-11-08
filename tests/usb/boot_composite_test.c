/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "keyboard_port.h"
#include "keyboard_rx.h"
#include "boot_hid.h"
typedef struct {unsigned index;unsigned short regs[3][0x90];unsigned char raw[0x500],fifo[3][8],reports[2][9];unsigned pos[3],count,released;KB_SESSION *session;} MOCK;
static unsigned char r8(void *p,unsigned a){MOCK*m=p;if(a==0x24||a==0x28){unsigned ch=(a-0x20)/4;return m->fifo[ch][m->pos[ch]++];}return m->raw[a];}
static unsigned short r16(void*p,unsigned a){MOCK*m=p;return a<0x90?m->regs[m->index][a]:m->raw[a]|((unsigned short)m->raw[a+1]<<8);}
static void w8(void*p,unsigned a,unsigned char v){MOCK*m=p;if(a==0xe)m->index=v;else m->raw[a]=v;assert(m->index<3);}
static void w16(void*p,unsigned a,unsigned short v){MOCK*m=p;if(a==0x16)m->regs[m->index][a]=(v&0x90)?0:(unsigned short)((m->regs[m->index][a]&v&0x4d)|(v&~0x4du));else if(a<0x90)m->regs[m->index][a]=v;else{m->raw[a]=(unsigned char)v;m->raw[a+1]=(unsigned char)(v>>8);}}
static void delay(void*p,unsigned ms){(void)p;(void)ms;}
static int report(void*p,const unsigned char*r){MOCK*m=p;assert(m->count<2);assert(!A04BootHidEncode(KbSessionReportProtocol(m->session),r,m->reports[m->count++]));return 0;}
static void release(void*p){++((MOCK*)p)->released;}
static void parse(KB_ENUM*k,unsigned char*c){unsigned char d[18]={18,1,0,2,0,0,0,8,0,0,0,0,0,0,0,0,0,1};KbEnumStart(k);assert(!KbControlComplete(k,d,8));assert(!KbControlComplete(k,0,0));assert(!KbControlComplete(k,d,18));assert(!KbControlComplete(k,c,9));}
int main(void){
 unsigned char config[]={9,2,41,0,2,3,0,0x80,50,9,4,7,0,1,3,1,2,0,7,5,0x83,3,3,0,10,9,4,8,0,1,3,1,1,0,7,5,0x84,3,8,0,1};
 KB_ENUM k;KB_CONTROL c;KB_SESSION s={0};KB_PORT p;MOCK m={0};MT8167_USB20_IO io={&m,r8,r16,w8,w16};unsigned now=0xfffffff0u;
 parse(&k,config);assert(!KbControlComplete(&k,config,sizeof(config)));assert(k.protocol==1&&k.interface_number==8&&k.endpoint==4&&k.secondary_interface==7&&k.secondary_endpoint==3);
 assert(!KbControlComplete(&k,0,0));assert(!KbNextControl(&k,&c)&&c.setup[1]==11&&c.setup[4]==8);assert(!KbControlComplete(&k,0,0)&&k.stage==KB_PROTOCOL_SECONDARY);assert(!KbNextControl(&k,&c)&&c.setup[4]==7);assert(!KbControlComplete(&k,0,0)&&k.stage==KB_READY);
 assert(!KbPortInit(&p,&s,&io,&m,delay,report,release));s.enumeration=k;s.io=io;s.speed=0x80;s.client=&m;s.report=report;m.session=&s;p.stage=KB_PORT_READY;p.next_poll=now;p.secondary_next_poll=now;
 assert(!KbRxConfigureChannel(&io,1,1,4,8,1,0x80,1));assert(!KbRxConfigureChannel(&io,2,1,3,3,10,0x80,2));assert(m.raw[0x48c]==1&&m.raw[0x494]==1);assert(m.regs[1][0x14]==8&&m.regs[2][0x14]==3);
 assert(!KbPortTick(&p,now)&&p.rx_pending&&p.secondary_rx_pending);assert(m.regs[1][0x16]&0x20);assert(m.regs[2][0x16]&0x20);
 m.regs[1][0x16]=8;assert(!KbPortEvents(&p,(MT8167_USB20_EVENTS){0,0,2},now));assert(!p.rx_pending&&p.secondary_rx_pending);
 m.regs[2][0x16]=1;m.regs[2][0x18]=3;memcpy(m.fifo[2],(unsigned char[]){1,0x80,0x7f},3);assert(!KbPortEvents(&p,(MT8167_USB20_EVENTS){0,0,4},now));assert(!p.secondary_rx_pending&&m.count==1&&m.reports[0][0]==2&&m.reports[0][2]==0x80);
 assert(!KbPortTick(&p,now+2)&&p.rx_pending);m.regs[1][0x16]=1;m.regs[1][0x18]=8;m.fifo[1][2]=4;assert(!KbPortEvents(&p,(MT8167_USB20_EVENTS){0,0,2},now+2));assert(m.count==2&&m.reports[1][0]==1&&m.reports[1][3]==4);
 assert(!KbPortTick(&p,now+300000)&&p.rx_pending&&p.secondary_rx_pending);assert(!KbPortEvents(&p,(MT8167_USB20_EVENTS){0x20,0,6},now+300001));assert(p.stage==KB_PORT_WAIT&&!p.rx_pending&&!p.secondary_rx_pending&&m.released==1);
 config[36]=0x83;parse(&k,config);assert(KbControlComplete(&k,config,sizeof(config))<0);config[36]=0x84;config[27]=7;parse(&k,config);assert(KbControlComplete(&k,config,sizeof(config))<0);
 return 0;
}
