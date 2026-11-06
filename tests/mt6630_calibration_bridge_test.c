/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "mt6630_nvram_fixture.h"
#include "../drivers/network/mt6630/wmt_bridge.c"
struct fake {unsigned reads,writes,fail;uint8_t packet[1024];};
static int bus_io(void *ctx,uint32_t arg,unsigned write,uint8_t *p,size_t n)
{
 struct fake *f=ctx;unsigned address=(arg>>9)&0x1ffff;
 if(write){assert(address==0x34&&n<=1024&&n>=8);memcpy(f->packet,p,n);f->writes++;return f->fail?9:0;}
 assert(n==4&&address>=0x130&&address<=0x14c);memset(p,0,n);f->reads++;return 0;
}
static int direct_io(void *ctx,uint32_t arg,uint32_t *v){(void)ctx;(void)arg;*v=0;return 0;}
static int done(void *p){(void)p;return 0;}
static void wait_us(void *p,unsigned us){(void)p;(void)us;}
int main(void)
{
 static struct a04_wmt_bridge b;static struct a04_nvram_work work;
 struct a04_nvram_reader r={0,sizeof(disk),readpart},sr;struct a04_nvram_sector_reader sectors;
 struct a04_wifi_owner owner={0,1,1,1,1,1,0,0};struct a04_wifi_life life={0};struct fake f={0};
 struct a04_sdio_direct d={&owner,&f,direct_io,0,0};struct a04_sdio_bus bus={&f,bus_io};
 struct a04_wifi_life_ops ops={&f,done,done,done,done};
 uint8_t cis[]={0x20,4,0x7a,3,0x30,0x66,0xff},data[512];
 make_fixture();assert(!a04_mt6630_calibration_read(&r,&work,data));
 assert(!a04_nvram_sector_init(&sectors,&sr,0,readsector,8,sizeof(disk)/512,8+sizeof(disk)/512));
 assert(!a04_mt6630_calibration_read(&sr,&work,data));
 assert(!a04_wifi_admit(&owner));assert(!a04_wifi_life_begin(&life,&owner,cis,sizeof(cis),&d,&bus,&ops));
 b.life=&life;b.wlan_ready=1;b.io.ctx=&b;b.io.command=command;b.io.delay=wait_us;
 b.transport.io=&b.io;b.transport.transfer=transfer;b.transport.enabled=2;b.transport.functions=2;
 assert(!a04_wifi_life_block_verified(&life,1,512));
 b.wlan.transport=&b.transport;b.wlan.ready=b.wlan.initialized=1;
 data[4]|=1;assert(a04_wmt_bridge_load_calibration(&b,data)&&!f.reads&&!f.writes);data[4]&=~1u;
 data[196]=0;assert(a04_wmt_bridge_load_calibration(&b,data)&&!f.reads&&!f.writes);data[196]=1;
 assert(!a04_wmt_bridge_load_calibration(&b,data));
 assert(b.calibration_submitted&&b.wlan.runtime&&f.reads==8&&f.writes==4);
 assert(a04_wmt_le16(f.packet)==520&&f.packet[4]==0x48&&f.packet[5]==0xa0&&f.packet[6]==1);
 assert(!memcmp(f.packet+8,data,512));for(unsigned i=520;i<1024;i++)assert(!f.packet[i]);
 for(unsigned i=8;i<520;i++)assert(!b.wlan.tx[i]);
 assert(a04_wmt_bridge_load_calibration(&b,data)&&f.writes==4);
 b.calibration_submitted=0;f.fail=1;
 assert(a04_wmt_bridge_load_calibration(&b,data)&&b.failed&&!b.calibration_submitted&&owner.poisoned);
 for(unsigned i=8;i<520;i++)assert(!b.wlan.tx[i]);
 assert(a04_wmt_bridge_load_calibration(&b,data)&&f.writes==5);
 puts("NVRAM -> TX power/domain/country limit/original record; wipe and poison: OK");return 0;
}
