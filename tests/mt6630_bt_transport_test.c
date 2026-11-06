/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One bounded protocol regression. Synthetic replies are not device evidence. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/bluetooth/mt6630/transport.h"
struct fixture {struct a04_wmt_bridge bridge;uint8_t response[2560],nvram[36];unsigned length,step,received;int reject;};
static struct fixture f;
static struct a04_bt_transport bt;
static int io_command(void *p,unsigned op,uint32_t arg,unsigned response,uint32_t *out)
{(void)p;(void)op;(void)arg;(void)response;*out=0;return 0;}
static void delay(void *p,unsigned us){(void)p;(void)us;}
static unsigned frame(uint8_t *p,unsigned task,const uint8_t *data,unsigned length)
{
 unsigned n=0;assert(!a04_stp_encode(A04_STP_SDIO,task,0,0,0,data,length,p,256,&n));return n;
}
static int transfer(void *context,uint32_t arg,void *buffer,unsigned length)
{
 struct fixture *s=context;unsigned addr=(arg>>9)&0x1ffff;uint8_t *p=buffer;
 assert(((arg>>28)&7)==2);
 if(!(arg>>31)){
  if(addr==4){a04_wmt_put32(p,0x100);return 0;}
  if(addr==0x10){a04_wmt_put32(p,s->length?(s->length<<16)|0x12:0);return 0;}
  assert(addr==0x1c&&s->length);memcpy(p,s->response,length);s->length=0;return 0;
 }
 assert(addr==0x18);struct a04_stp_frame stp;
 assert(!a04_stp_parse(A04_STP_SDIO,p+4,a04_wmt_le16(p)-4,&stp));
 memset(s->response,0,sizeof(s->response));unsigned off=4;
 if(stp.task==4){
  assert(stp.payload.length==6&&stp.payload.data[1]==6);
  const uint8_t notification[]={2,0x16,1,0,0},ack[]={2,6,1,0,0};
  off+=frame(s->response+off,4,notification,sizeof(notification));off=(off+3)&~3u;
  off+=frame(s->response+off,4,ack,sizeof(ack));
 }else{
  static const unsigned ops[]={0xfc1a,0xfc72,0xfc79,0xfc93,0xfc7a,0xfc22,0x1001};
  static const unsigned offsets[]={0,8,12,27,18,30,0},sizes[]={6,4,6,3,7,6,0};
  const uint8_t *h=stp.payload.data;unsigned op=a04_wmt_le16(h+1),step=s->step++;
  assert(stp.task==0&&h[0]==1&&step<7&&op==ops[step]&&h[3]==sizes[step]);
  for(unsigned i=0;i<sizes[step];i++)assert(h[4+i]==s->nvram[step?offsets[step]+i:5-i]);
  uint8_t reply[15]={4,0x0e,4,1,(uint8_t)op,(uint8_t)(op>>8),0};unsigned n=7;
  if(s->reject)reply[6]=0x0c;
  if(op==0x1001){const uint8_t version[]={6,0x34,0x12,6,0x46,0,0x78,0x56};memcpy(reply+7,version,8);reply[2]=12;n=15;}
  /* Exercise H4 reconstruction over STP boundaries, not just whole packets. */
  off+=frame(s->response+off,0,reply,2);off=(off+3)&~3u;
  off+=frame(s->response+off,0,reply+2,n-2);
 }
 a04_wmt_put16(s->response,off);s->length=off;return 0;
}
static int receive(void *context,unsigned type,const uint8_t *p,unsigned length)
{struct fixture *s=context;(void)type;(void)p;(void)length;s->received++;return 0;}
static void init(void)
{
 memset(&f,0,sizeof(f));for(unsigned i=0;i<36;i++)f.nvram[i]=(uint8_t)(i+2);
 f.bridge.wmt_ready=1;f.bridge.channel.initialized=1;f.bridge.channel.free_bytes=2080;
 f.bridge.io.ctx=&f;f.bridge.io.command=io_command;f.bridge.io.delay=delay;
 f.bridge.transport.io=&f.bridge.io;f.bridge.transport.enabled=4;f.bridge.transport.transfer=transfer;
 assert(!a04_bt_open(&bt,&f.bridge,&f,receive));
}
int main(void)
{
 init();assert(!a04_bt_initialize(&bt,f.nvram));assert(bt.ready&&f.step==7&&!f.received);
 assert(bt.version.hci_revision==0x1234&&bt.version.manufacturer==0x46&&bt.version.lmp_subversion==0x5678);
 assert(!a04_bt_disable(&bt)&&!bt.enabled&&!bt.ready);
 init();f.reject=1;assert(a04_bt_initialize(&bt,f.nvram)&&!bt.ready&&bt.failed);
 puts("Synthetic MT6630 BT init, aggregate notification, fragmented H4 and failure propagation: OK");return 0;
}
