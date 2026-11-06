/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Protocol facts: Linux a04br3_mt6630_core.c / a04br3_mt6630_bt.c,
 * revision 3d2c74e252d6f34a5980deda96bdc04064108d74. Independent implementation.
 */
#include "transport.h"
#include <string.h>
static int fail(struct a04_bt_transport *b,int e)
{b->failed=e;b->ready=0;b->bridge->failed=1;return e;}
static int valid(struct a04_bt_transport *b)
{return b&&b->bridge&&b->bridge->wmt_ready&&!b->bridge->failed&&!b->failed;}
static int own(struct a04_bt_transport *b)
{
 uint32_t v;uint8_t request=2;
 if(a04_sdio_read32(&b->bridge->transport,2,4,&v))return fail(b,-1);
 if(v&0x100)return 0;
 /* CHLPCR upper byte via CMD52: vendor COHEC_00006052 workaround. */
 if(a04_sdio_direct(&b->bridge->io,2,5,1,&request))return fail(b,-1);
 for(unsigned i=0;i<1200;i++){
  if(a04_sdio_read32(&b->bridge->transport,2,4,&v))return fail(b,-1);
  if(v&0x100)return 0;
  b->bridge->io.delay(b->bridge->io.ctx,500);
 }
 return fail(b,-2);
}
static int hci_packet(struct a04_bt_transport *b)
{
 unsigned n=b->hci_expected-1,type=b->hci[0];const uint8_t *p=b->hci+1;
 if(type==4&&b->awaiting){
  /* Command Complete: command credits, opcode, return parameters. A matching
   * Command Status is only acceptance, never completion of initialization. */
  if(n>=5&&p[0]==0x0e&&a04_wmt_le16(p+3)==b->awaiting){
   if(b->complete||n<6||p[5]||n-5>sizeof(b->reply))return fail(b,-3);
   b->reply_length=n-5;memcpy(b->reply,p+5,b->reply_length);b->complete=1;return 0;
  }
  if(n>=6&&p[0]==0x0f&&a04_wmt_le16(p+4)==b->awaiting){if(p[2])return fail(b,-3);return 0;}
 }
 return b->receive?b->receive(b->context,type,p,n):fail(b,-4);
}
static int hci_bytes(struct a04_bt_transport *b,const uint8_t *p,unsigned n)
{
 for(unsigned i=0;i<n;i++){
  if(b->hci_used>=sizeof(b->hci))return fail(b,-5);
  b->hci[b->hci_used++]=p[i];
  unsigned type=b->hci[0];
  if(type!=4&&type!=2)return fail(b,-5); /* SCO/ISO not advertised */
  if(type==4&&b->hci_used==3)b->hci_expected=3+b->hci[2];
  if(type==2&&b->hci_used==5)b->hci_expected=5+a04_wmt_le16(b->hci+3);
  if(b->hci_expected>sizeof(b->hci))return fail(b,-5);
  if(b->hci_expected&&b->hci_used==b->hci_expected){
   int e=hci_packet(b);b->hci_used=b->hci_expected=0;if(e)return fail(b,e);
  }
 }
 return 0;
}
static int wmt_bytes(struct a04_bt_transport *b,const uint8_t *p,unsigned n)
{
 for(unsigned i=0;i<n;i++){
  if(b->wmt_used>=sizeof(b->wmt))return fail(b,-6);
  b->wmt[b->wmt_used++]=p[i];
  if(b->wmt_used==4)b->wmt_expected=4+a04_wmt_le16(b->wmt+2);
  if(b->wmt_expected>sizeof(b->wmt)|| (b->wmt_used==4&&b->wmt_expected<5))return fail(b,-6);
  if(b->wmt_expected&&b->wmt_used==b->wmt_expected){
   if(b->wmt[0]!=2)return fail(b,-6);
   if(b->wmt[1]==6){
    if(!b->func_wait||b->func_complete||b->wmt_expected!=5||b->wmt[4])return fail(b,-6);
    b->func_complete=1;
   }else if(b->wmt[1]!=0x16)return fail(b,-6); /* explicit async notification */
   b->wmt_used=b->wmt_expected=0;
  }
 }
 return 0;
}
int a04_bt_poll(struct a04_bt_transport *b)
{
 uint32_t status;int e;struct a04_wmt_sdio_cursor cursor;struct a04_stp_frame frame;
 if(!valid(b))return -1;
 if(own(b))return b->failed;
 if(a04_sdio_read32(&b->bridge->transport,2,0x10,&status))return fail(b,-1);
 if(status&0xff80)return fail(b,-7);
 unsigned done=(status>>4)&7;
 if(done){if(done!=1||!b->pending)return fail(b,-8);b->pending=0;}
 if(!(status&2))return 0;
 unsigned length=status>>16,bus=(length+3)&~3u;if(bus>512)bus=(bus+511)&~511u;
 if(length<=4||length>2304)return fail(b,-9);
 if(a04_xnu_sdio_transfer(&b->bridge->transport,2,0x1c,0,0,b->rx,bus))return fail(b,-1);
 if(a04_wmt_sdio_open(b->rx,bus,length,&cursor))return fail(b,-9);
 while((e=a04_wmt_sdio_next(&cursor,&frame))==0){
  if(frame.task==0){if(hci_bytes(b,frame.payload.data,frame.payload.length))return b->failed;}
  else if(frame.task==4){if(wmt_bytes(b,frame.payload.data,frame.payload.length))return b->failed;}
  else return fail(b,-7); /* assert/log/other combo traffic is not HCI */
 }
 return e==A04_WMT_DONE?0:fail(b,-9);
}
static int idle(struct a04_bt_transport *b)
{
 for(unsigned i=0;i<1000;i++){
  if(a04_bt_poll(b))return b->failed;
  if(!b->pending)return 0;
  b->bridge->io.delay(b->bridge->io.ctx,1000);
 }
 return fail(b,-2);
}
static int send(struct a04_bt_transport *b,unsigned task,const uint8_t *p,unsigned n)
{
 unsigned written,logical=n+10,fifo=(logical+3)&~3u,bus=fifo>512?(fifo+511)&~511u:fifo;
 if(!valid(b)||!p||!n||n>A04_STP_MAX_PAYLOAD||fifo>2080)return -1;
 if(idle(b))return b->failed;
 memset(b->tx,0,bus);a04_wmt_put16(b->tx,logical);
 if(a04_stp_encode(A04_STP_SDIO,task,0,0,0,p,n,b->tx+4,sizeof(b->tx)-4,&written))return -1;
 b->pending=fifo;
 if(a04_xnu_sdio_transfer(&b->bridge->transport,2,0x18,1,0,b->tx,bus))return fail(b,-1);
 return 0;
}
int a04_bt_write(struct a04_bt_transport *b,unsigned type,const uint8_t *p,unsigned n)
{
 uint8_t h4[A04_STP_MAX_PAYLOAD];int e;
 if(!valid(b)||!b->enabled||!p||n>A04_BT_MAX_HCI)return -1;
 if(type==1){if(n<3||n!=3u+p[2])return -1;}
 else if(type==2){if(n<4||n!=4u+a04_wmt_le16(p+2))return -1;}
 else return -1;
 h4[0]=(uint8_t)type;memcpy(h4+1,p,n);e=send(b,0,h4,n+1);
 /* Volatile clearing preserves erasure for commands containing keys. */
 for(volatile unsigned char *v=h4;v<h4+sizeof(h4);v++)*v=0;
 for(volatile unsigned char *v=b->tx;v<b->tx+sizeof(b->tx);v++)*v=0;
 return e;
}
static int function(struct a04_bt_transport *b,unsigned on)
{
 uint8_t cmd[6]={1,6,2,0,0,(uint8_t)on};
 b->func_wait=0;b->func_complete=0;
 if(send(b,4,cmd,sizeof(cmd)))return b->failed;
 b->func_wait=1;
 for(unsigned i=0;i<2000;i++){
  if(a04_bt_poll(b))return b->failed;
  if(b->func_complete&&!b->pending){b->func_wait=0;b->enabled=on;return 0;}
  b->bridge->io.delay(b->bridge->io.ctx,1000);
 }
 return fail(b,-2);
}
static int command(struct a04_bt_transport *b,unsigned op,const uint8_t *p,unsigned n)
{
 uint8_t cmd[10];int e;
 if(n>7)return -1;
 /* Drain old traffic before arming the opcode match. */
 if(idle(b))return b->failed;
 a04_wmt_put16(cmd,op);cmd[2]=(uint8_t)n;if(n)memcpy(cmd+3,p,n);
 b->awaiting=0;b->complete=b->reply_length=0;e=a04_bt_write(b,1,cmd,n+3);
 for(volatile unsigned char *v=cmd;v<cmd+sizeof(cmd);v++)*v=0;
 if(e)return e;
 b->awaiting=op;
 for(unsigned i=0;i<2000;i++){
  if(a04_bt_poll(b))return b->failed;
  if(b->complete&&!b->pending){b->awaiting=0;return 0;}
  b->bridge->io.delay(b->bridge->io.ctx,1000);
 }
 return fail(b,-2);
}
int a04_bt_open(struct a04_bt_transport *b,struct a04_wmt_bridge *bridge,void *p,int (*rx)(void *,unsigned,const uint8_t *,unsigned))
{
 if(!b||!bridge||!bridge->wmt_ready||bridge->failed||bridge->wlan_ready||
    !bridge->channel.initialized||bridge->channel.pending_cost||bridge->channel.free_bytes!=2080)return -1;
 memset(b,0,sizeof(*b));b->bridge=bridge;b->context=p;b->receive=rx;
 /* Bootstrap channel must never read CHISR again after this handoff. */
 bridge->channel.initialized=0;return 0;
}
int a04_bt_initialize(struct a04_bt_transport *b,const uint8_t nv[36])
{
 uint8_t addr[6];unsigned any=0,all=255;int e;
 if(!valid(b)||!nv||b->enabled)return -1;
 for(unsigned i=0;i<6;i++){addr[i]=nv[5-i];any|=nv[i];all&=nv[i];}
 /* NVRAM uses human address order. Never invent a fallback address. */
 if(!any||all==255||(nv[0]&1))return -1;
 e=function(b,1);
 if(!e)e=command(b,0xfc1a,addr,6);
 for(volatile unsigned char *v=addr;v<addr+6;v++)*v=0;
 if(e||command(b,0xfc72,nv+8,4)||command(b,0xfc79,nv+12,6)||
    command(b,0xfc93,nv+27,3)||command(b,0xfc7a,nv+18,7)||command(b,0xfc22,nv+30,6)||
    command(b,0x1001,NULL,0))return fail(b,-10);
 if(b->reply_length!=9)return fail(b,-11);
 b->version.hci_version=b->reply[1];b->version.hci_revision=(uint16_t)a04_wmt_le16(b->reply+2);
 b->version.lmp_version=b->reply[4];b->version.manufacturer=(uint16_t)a04_wmt_le16(b->reply+5);
 b->version.lmp_subversion=(uint16_t)a04_wmt_le16(b->reply+7);b->ready=1;return 0;
}
int a04_bt_disable(struct a04_bt_transport *b)
{if(!valid(b))return -1;b->ready=0;return b->enabled?function(b,0):idle(b);}
