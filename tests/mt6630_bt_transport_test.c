/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One bounded protocol regression. Synthetic replies are not device evidence. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/bluetooth/mt6630/transport.h"
struct fixture {struct a04_wmt_bridge bridge;uint8_t response[2560],nvram[36];unsigned length,step,received,tx_done;uint64_t now;int reject;};
static struct fixture f;
static struct a04_bt_transport bt;
static int io_command(void *p,unsigned op,uint32_t arg,unsigned response,uint32_t *out)
{(void)p;(void)op;(void)arg;(void)response;*out=0;return 0;}
static void delay(void *p,unsigned us){((struct fixture *)p)->now+=us;}
static uint64_t now_us(void *p){return ((struct fixture *)p)->now;}
static unsigned frame(uint8_t *p,unsigned task,const uint8_t *data,unsigned length)
{
 unsigned n=0;assert(!a04_stp_encode(A04_STP_SDIO,task,0,0,0,data,length,p,256,&n));return n;
}
static int transfer(void *context,uint32_t arg,void *buffer,unsigned length)
{
 struct fixture *s=context;unsigned addr=(arg>>9)&0x1ffff;uint8_t *p=buffer;
 assert(((arg>>28)&7)==2);
 if(!(arg>>31)){
  if(addr==4){if(s->reject==3)s->now+=600001; a04_wmt_put32(p,0x100);return 0;}
  if(addr==0x10){a04_wmt_put32(p,(s->length?(s->length<<16)|2u:0)|(s->tx_done?0x10u:0));s->tx_done=0;return 0;}
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
  static const unsigned ops[]={0xfc1a,0xfc72,0xfc79,0xfc93,0xfc7a,0xfc22,0x1001,0x1009};
  static const unsigned offsets[]={0,8,12,27,18,30,0,0},sizes[]={6,4,6,3,7,6,0,0};
  const uint8_t *h=stp.payload.data;unsigned op=a04_wmt_le16(h+1),step=s->step++;
  assert(stp.task==0&&h[0]==1&&step<8&&op==ops[step]&&h[3]==sizes[step]);
  for(unsigned i=0;i<sizes[step];i++)assert(h[4+i]==s->nvram[step?offsets[step]+i:5-i]);
  uint8_t reply[15]={4,0x0e,4,1,(uint8_t)op,(uint8_t)(op>>8),0};unsigned n=7;
  if(s->reject==1||(s->reject==2&&op==0x1001))reply[6]=0x0c;
  if(op==0x1001){const uint8_t version[]={6,0x34,0x12,6,0x46,0,0x78,0x56};memcpy(reply+7,version,8);reply[2]=12;n=15;}
  if(op==0x1009){
   for(unsigned i=0;i<6;i++)reply[7+i]=s->nvram[5-i];
   reply[2]=10;n=13;
   if(s->reject==4)reply[7]^=1; /* accepted FC1A but wrong controller value */
   if(s->reject==5){reply[2]=9;n=12;} /* short successful reply */
   if(s->reject==6)reply[6]=0x0c; /* actual controller rejection */
  }
  /* Exercise H4 reconstruction over STP boundaries, not just whole packets. */
  off+=frame(s->response+off,0,reply,2);off=(off+3)&~3u;
  off+=frame(s->response+off,0,reply+2,n-2);
 }
 a04_wmt_put16(s->response,off);s->length=off;s->tx_done=1;return 0;
}
static int receive(void *context,unsigned type,const uint8_t *p,unsigned length)
{struct fixture *s=context;(void)type;(void)p;(void)length;s->received++;return 0;}
static void init(void)
{
 memset(&f,0,sizeof(f));for(unsigned i=0;i<36;i++)f.nvram[i]=(uint8_t)(i+2);
 f.bridge.wmt_ready=1;f.bridge.channel.initialized=1;f.bridge.channel.free_bytes=2080;
 f.bridge.io.ctx=&f;f.bridge.io.command=io_command;f.bridge.io.delay=delay;
 f.bridge.transport.io=&f.bridge.io;f.bridge.transport.enabled=4;f.bridge.transport.transfer=transfer;
 assert(!a04_bt_open(&bt,&f.bridge,&f,receive,now_us));
}
static void inject(const uint8_t *event,unsigned bytes)
{
 memset(f.response,0,sizeof(f.response));unsigned off=4+frame(f.response+4,0,event,bytes);
 a04_wmt_put16(f.response,off);f.length=off;assert(!a04_bt_poll(&bt));
}
int main(void)
{
 init();assert(!a04_bt_initialize(&bt,f.nvram));assert(bt.ready&&f.step==8&&!f.received);
 assert(bt.version_sent&&bt.version_complete&&bt.version_status==0&&bt.version_length==9&&bt.version_valid);
 assert(bt.version.hci_revision==0x1234&&bt.version.manufacturer==0x46&&bt.version.lmp_subversion==0x5678);
 assert(bt.pcm_command_applied&&!bt.queue_stopped);
 for(unsigned i=0;i<sizeof(bt.reply);i++)assert(!bt.reply[i]);
 assert(!bt.reply_length);
 /* Request acceptance is not a connection; only actual matching HCI events
  * update the private state. No address from this fixture is logged. */
 const uint8_t accepted[]={4,0x0f,4,0,1,0x28,4};inject(accepted,sizeof(accepted));assert(!bt.sco.active);
 const uint8_t connected[]={4,0x2c,17,0,0x40,0,0,0,0,0,0,0,2,6,2,60,0,60,0,2};
 inject(connected,sizeof(connected));assert(bt.sco.active&&bt.sco.connections==1&&bt.sco.air_mode==2&&bt.sco.rx_bytes==60);
 const uint8_t disconnected[]={4,5,4,0,0x40,0,0x13};inject(disconnected,sizeof(disconnected));assert(!bt.sco.active&&bt.sco.disconnections==1);
 assert(!a04_bt_disable(&bt)&&!bt.enabled&&!bt.ready&&bt.version_valid&&bt.queue_stopped&&!bt.pcm_command_applied);
 init();assert(!a04_bt_initialize(&bt,f.nvram));bt.hci_used=1;
 assert(a04_bt_disable(&bt)==-2&&!bt.queue_stopped); /* partial packet is NOT a drained queue */
 init();f.reject=1;assert(a04_bt_initialize(&bt,f.nvram)&&!bt.ready&&bt.failed&&!bt.version_sent);
 init();f.reject=2;assert(a04_bt_initialize(&bt,f.nvram)&&!bt.version_valid&&bt.version_sent&&bt.version_complete&&bt.version_status==0x0c);
 init();f.reject=3;assert(a04_bt_initialize(&bt,f.nvram)&&bt.failed==-2&&!bt.ready&&!bt.version_sent);
 for(int rejected=4;rejected<=6;rejected++){
  init();f.reject=rejected;assert(a04_bt_initialize(&bt,f.nvram)&&!bt.ready&&bt.failed&&bt.version_valid);
  if(rejected!=6)assert(bt.failed==-13);
  for(unsigned i=0;i<sizeof(bt.reply);i++)assert(!bt.reply[i]);
  assert(!bt.reply_length);
 }
 /* Shared attach preserves WLAN and transfers bootstrap fn2 consumption
  * once. Every fake transfer asserts fn2; no fn1 FIFO/status read is made. */
 init();f.bridge.wlan_ready=1;
 assert(a04_bt_open(&bt,&f.bridge,&f,receive,now_us));
 assert(a04_bt_open_combo(&bt,&f.bridge,&f,receive,now_us));
 assert(f.bridge.channel.initialized==0); /* init() attached exclusive owner */
 f.bridge.channel.initialized=1;f.bridge.calibration_submitted=1;
 assert(!a04_bt_open_combo(&bt,&f.bridge,&f,receive,now_us));
 assert(!f.bridge.channel.initialized&&!a04_bt_initialize(&bt,f.nvram));
 assert(f.bridge.wlan_ready&&f.bridge.calibration_submitted&&!f.bridge.failed);
 assert(!a04_bt_disable(&bt));f.step=0;
 assert(!a04_bt_open_combo(&bt,&f.bridge,&f,receive,now_us));
 assert(!a04_bt_initialize(&bt,f.nvram));
 assert(f.bridge.wlan_ready&&f.bridge.calibration_submitted);
 puts("Synthetic MT6630 BT init, aggregate notification, fragmented H4 and failure propagation: OK");return 0;
}
