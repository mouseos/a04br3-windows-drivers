/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "wmt_bridge.h"
static int alive(const struct a04_wmt_bridge *b)
{ return b && b->life && b->life->state==A04_WIFI_LIFE_IO && b->life->owner &&
 b->life->owner->active && !b->life->owner->poisoned &&
 b->life->generation==b->life->owner->generation; }
static int fault(struct a04_wmt_bridge *b,int e)
{
 b->failed=1;b->wmt_ready=b->wlan_ready=0;
 if(b->life && (b->life->state==A04_WIFI_LIFE_IO || b->life->state==A04_WIFI_LIFE_FAILED)){b->life->state=A04_WIFI_LIFE_FAILED;b->life->transport.ready=0;
  if(b->life->owner && !b->life->owner->poisoned)a04_wifi_fault(b->life->owner);}
 return e?e:-1;
}
static int command(void *context,unsigned op,uint32_t arg,unsigned response,uint32_t *out)
{
 struct a04_wmt_bridge *b=context;uint8_t value=(uint8_t)arg;int e;
 if(op!=52 || response!=1 || !out)return -1;
 e=a04_wifi_life_cmd52(b->life,arg>>31,(arg>>28)&7,(arg>>27)&1,(arg>>9)&0x1ffff,&value);
 if(e)return e;
 *out=value;return 0;
}
static void delay_us(void *context,unsigned us)
{struct a04_wmt_bridge *b=context;b->delay(b->delay_context,us);}
static int transfer(void *context,uint32_t arg,void *buffer,unsigned length)
{
 struct a04_wmt_bridge *b=context;
 return a04_wifi_life_cmd53(b->life,arg>>31,(arg>>28)&7,(arg>>26)&1,(arg>>9)&0x1ffff,buffer,length);
}
static int verify_block(struct a04_wmt_bridge *b,unsigned fn)
{
 uint8_t lo=0,hi=0;unsigned base=fn<<8;
 if(a04_wifi_life_cmd52(b->life,0,0,0,base+0x10,&lo) ||
    a04_wifi_life_cmd52(b->life,0,0,0,base+0x11,&hi) || lo || hi!=2)return -1;
 return a04_wifi_life_block_verified(b->life,fn,512);
}
int a04_wmt_bridge_open(struct a04_wmt_bridge *b,struct a04_wifi_life *life,unsigned functions,void *context,void (*delay)(void *,unsigned))
{
 uint32_t hw,rom;int e;struct a04_sdio_wmt_state state={0};
 if(!b||!life||!delay||functions<2||functions>7||life->state!=A04_WIFI_LIFE_IO || b->identity_ready || b->failed)return -1;
 b->life=life;b->delay_context=context;b->delay=delay;b->io.ctx=b;b->io.command=command;b->io.delay=delay_us;
 b->transport.io=&b->io;b->transport.transfer=transfer;b->transport.functions=functions;b->transport.enabled=0;b->transport.failed=0;
 e=a04_sdio_wmt_prepare(&b->transport,&state);if(e)return fault(b,e);
 if(verify_block(b,2))return fault(b,-1);
 e=a04_wmt_channel_init(&b->transport,&b->channel);if(e)return fault(b,e);
 e=a04_wmt_read_version(&b->transport,&b->channel,0x80000000u,&hw);if(e)return fault(b,e);
 e=a04_wmt_read_version(&b->transport,&b->channel,0x80000004u,&rom);if(e)return fault(b,e);
 b->identity.chip=0x6630;b->identity.hardware=hw&0xffffu;b->identity.rom=rom&0xffffu;b->identity_ready=1;return 0;
}
int a04_wmt_bridge_boot_wmt(struct a04_wmt_bridge *b,const void *data,unsigned bytes,
 const uint8_t expected[32],int (*sha256)(const uint8_t *,size_t,uint8_t[32]),
 const struct a04_wmt_startup_options *options)
{
 uint8_t digest[32];unsigned different=0,i;int e;
 if(!alive(b)||!b->identity_ready||b->failed||!data||!expected||!sha256||!options||b->wmt_ready)return -1;
 if(sha256(data,bytes,digest))return -1;
 for(i=0;i<32;i++)different|=digest[i]^expected[i];
 if(different)return -1;
 e=a04_wmt_firmware_bundle(&b->identity,data,bytes,&b->wmt_fw);if(e)return e;
 e=a04_wmt_download_run(&b->transport,&b->channel,&b->wmt_fw,&b->identity,options,&b->wmt_state,0,0);
 b->wmt_fw.count=0; /* discard borrowed views after synchronous transfer */
 if(e)return fault(b,e);
 b->wmt_ready=b->wmt_state.stp_ready;return b->wmt_ready?0:fault(b,-1);
}
int a04_wmt_bridge_boot_wlan(struct a04_wmt_bridge *b,const void *data,unsigned bytes,int (*sha256)(const uint8_t *,size_t,uint8_t[32]))
{
 uint8_t digest[32];int e;
 if(!alive(b)||!b->wmt_ready||!b->wmt_state.function1_ready||b->failed||!data||!sha256||b->wlan_ready)return -1;
 if(sha256(data,bytes,digest))return -1;
 e=a04_wlan_firmware_select(&b->identity,data,bytes,digest,&b->wlan_fw);if(e)return e;
 e=a04_wlan_host_open(&b->wlan,&b->transport);if(e)return fault(b,e);
 if(verify_block(b,1))return fault(b,-1);
 e=a04_wlan_download(&b->wlan,&b->wlan_fw,&b->wlan_state);b->wlan_fw.count=0;if(e)return fault(b,e);
 b->wlan_ready=b->wlan_state.ready;return b->wlan_ready?0:fault(b,-1);
}

int a04_wmt_bridge_poll_wlan_status(struct a04_wmt_bridge *b,uint32_t *status)
{
 uint32_t observed;int e;
 if(!alive(b)||!b->wlan_ready||b->failed||!status||b->pending_wlan_status)return -1;
 e=a04_wlan_interrupt_status(&b->wlan,&observed);if(e)return fault(b,e);
 b->pending_wlan_status=observed;*status=observed;return 0;
}
int a04_wmt_bridge_receive(struct a04_wmt_bridge *b,unsigned port,unsigned polls,
 const uint8_t **data,unsigned *length)
{
 int e;if(!alive(b)||!b->wlan_ready||b->failed||!b->pending_wlan_status||!data||!length||port>1||!polls||polls>20000)return -1;
 e=a04_wlan_receive(&b->wlan,port,polls);if(e)return fault(b,e);
 *data=b->wlan.rx;*length=b->wlan.rx_length;return 0;
}
int a04_wmt_bridge_ack_wlan_status(struct a04_wmt_bridge *b,uint32_t handled)
{
 if(!alive(b) || !b->wlan_ready || b->failed || !b->pending_wlan_status || handled!=b->pending_wlan_status)return -1;
 b->pending_wlan_status=0;return 0;
}
