/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Exercise BT OFF through the real shared lifetime CMD53 gate. Synthetic
 * SDIO replies prove ordering/failure propagation, not hardware shutdown. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/bluetooth/mt6630/transport.h"
struct fixture {
 struct a04_wifi_owner owner;struct a04_wifi_life life;
 struct a04_wmt_bridge bridge;struct a04_bt_transport bt;
 uint8_t rx[64];unsigned bytes,done,writes,unmapped,released;
 uint64_t now;int reject;
};
static struct fixture f;
static int bus(void *p,uint32_t arg,unsigned write,uint8_t *data,size_t n)
{
 struct fixture *s=p;unsigned addr=(arg>>9)&0x1ffff;
 assert(((arg>>28)&7)==2&&write==(arg>>31));
 if(write){
  struct a04_stp_frame frame;unsigned written;const uint8_t ack[]={2,6,1,0,0};
  assert(addr==0x18);assert(!a04_stp_parse(A04_STP_SDIO,data+4,a04_wmt_le16(data)-4,&frame));
  assert(frame.task==4&&frame.payload.length==6&&frame.payload.data[5]==0);
  s->writes++;if(s->reject)return -1;
  assert(!a04_stp_encode(A04_STP_SDIO,4,0,0,0,ack,sizeof(ack),s->rx+4,sizeof(s->rx)-4,&written));
  s->bytes=written+4;a04_wmt_put16(s->rx,s->bytes);s->done=1;
 }else if(addr==4){a04_wmt_put32(data,0x100);}
 else if(addr==0x10){a04_wmt_put32(data,(s->bytes?(s->bytes<<16)|2u:0)|(s->done?0x10u:0));s->done=0;}
 else {assert(addr==0x1c&&s->bytes&&n<=sizeof(s->rx));memcpy(data,s->rx,n);s->bytes=0;}
 return 0;
}
static int gated(void *p,uint32_t arg,void *data,unsigned n)
{
 struct fixture *s=p;
 return a04_wifi_life_cmd53(&s->life,arg>>31,(arg>>28)&7,(arg>>26)&1,(arg>>9)&0x1ffff,data,n);
}
static uint64_t now(void *p){return ((struct fixture *)p)->now;}
static void delay(void *p,unsigned us){((struct fixture *)p)->now+=us;}
static int mask(void *p){(void)p;return 0;}
static int off_in_callback(void *p){return a04_bt_disable(&((struct fixture *)p)->bt);}
static int drained(void *p)
{
 struct fixture *s=p;
 assert(s->life.state==A04_WIFI_LIFE_STOPPING);
 return !s->bt.queue_stopped||s->bt.enabled||s->bt.pending||!s->bt.last_rx_empty||s->bt.hci_used||s->bt.wmt_used;
}
static int unmap(void *p){((struct fixture *)p)->unmapped++;return 0;}
static int release(void *p){((struct fixture *)p)->released++;return 0;}
static void init(void)
{
 memset(&f,0,sizeof(f));f.owner.active=1;f.owner.generation=7;
 f.life.owner=&f.owner;f.life.generation=7;f.life.state=A04_WIFI_LIFE_IO;
 f.life.transport.ready=1;f.life.transport.bus.context=&f;f.life.transport.bus.request=bus;
 f.life.ops=(struct a04_wifi_life_ops){&f,mask,drained,unmap,release};
 f.bridge.wmt_ready=1;f.bridge.io.ctx=&f;f.bridge.io.delay=delay;
 f.bridge.transport.io=&f.bridge.io;f.bridge.transport.enabled=4;f.bridge.transport.transfer=gated;
 f.bt.bridge=&f.bridge;f.bt.context=&f;f.bt.now_us=now;f.bt.enabled=f.bt.ready=1;
}
int main(void)
{
 /* Old order: lifetime revokes I/O before callback's first CHLPCR read. */
 init();f.life.ops.drain_io=off_in_callback;
 assert(a04_wifi_life_stop(&f.life)==-2);
 assert(f.life.state==A04_WIFI_LIFE_STOP_FAILED&&!f.writes&&!f.unmapped&&!f.released&&!f.bt.queue_stopped);
 /* New order: real OFF ACK, TX completion, fresh RX-empty before revoke. */
 init();assert(!a04_bt_disable(&f.bt));assert(f.writes==1&&f.bt.queue_stopped);
 assert(!a04_wifi_life_stop(&f.life));assert(f.unmapped==1&&f.released==1&&!f.owner.active);
 /* Missing OFF evidence cannot cross the generic stop/release boundary. */
 init();assert(a04_wifi_life_stop(&f.life)==-2&&!f.unmapped&&!f.released);
 /* Physical I/O failure is not an OFF ACK and retains ownership. */
 init();f.reject=1;assert(a04_bt_disable(&f.bt));assert(!f.bt.queue_stopped&&!f.unmapped&&!f.released&&f.owner.poisoned);
 puts("MT6630 BT lifetime gate: old OFF blocked; pre-stop OFF/drain succeeds; failure retains lease (synthetic)");
 return 0;
}
