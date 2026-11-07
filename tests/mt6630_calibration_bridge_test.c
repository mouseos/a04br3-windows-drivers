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
struct observation {unsigned count,stage[3],detail[3];int raw[3];};
static void observe(void *p,unsigned stage,int raw,unsigned detail)
{
 struct observation *o=p;assert(o->count<3);unsigned i=o->count++;
 o->stage[i]=stage;o->raw[i]=raw;o->detail[i]=detail;
}
struct codec_check {const uint8_t *record;unsigned ac,tx;};
static int codec(void *p,unsigned cid,const uint8_t *data,unsigned bytes)
{
 struct codec_check *c=p;
 if(cid==0x42){assert(bytes==12&&data[0]==1&&!memcmp(data+1,c->record+144,11));c->ac++;}
 if(cid==0x24){assert(bytes==40&&!memcmp(data,c->record+12,40));c->tx++;}
 return 0;
}
static void regulatory_cases(const uint8_t original[512])
{
 uint8_t record[512];struct a04_regulatory r;struct a04_regulatory_inspection d;
 memcpy(record,original,512);record[10]=record[11]=0;
 assert(!a04_regulatory_decode_inspect(record,&r,&d)&&d.selection==A04_REG_DEFAULT_TABLE);
 assert(!r.country[0]&&!r.country[1]&&!r.domain[0]&&!r.domain[1]&&r.permitted==0x3ffe&&r.active==0x3ffe);
 record[2]=1;record[3]=2;assert(a04_regulatory_decode_inspect(record,&r,&d)&&d.reason==A04_REG_VERSION);
 record[2]=record[3]=0;record[202]=1;record[203]=21;
 assert(!a04_regulatory_decode_inspect(record,&r,&d)&&d.selection==A04_REG_INDEX_TABLE);
 record[202]=2;record[203]=0;memset(record+204,0,36);record[205]=1; /* disabled first 2G subband */
 record[210]=81;record[211]=1;record[212]=1;record[213]=1;record[214]=11;
 assert(!a04_regulatory_decode_inspect(record,&r,&d)&&r.permitted==0xffe);
 record[214]=15;assert(a04_regulatory_decode_inspect(record,&r,&d)&&d.reason==A04_REG_GEOMETRY);
 memcpy(record,original,512);record[154]=0x35;record[155]=1;struct codec_check c={record,0,0};
 assert(!a04_regulatory_decode(record,&r));assert(!a04_manufacture_submit(record,&r,&c,codec)&&c.ac==1&&c.tx==1);
 record[155]=0;c.ac=0;assert(!a04_manufacture_submit(record,&r,&c,codec)&&!c.ac);
 record[0]=1;record[1]=0;c.tx=0;assert(!a04_manufacture_submit(record,&r,&c,codec)&&!c.tx);
}
int main(void)
{
 static struct a04_wmt_bridge b;static struct a04_nvram_work work;
 struct a04_nvram_reader r={0,sizeof(disk),readpart},sr;struct a04_nvram_sector_reader sectors;
 struct a04_wifi_owner owner={0,1,1,1,1,1,0,0};struct a04_wifi_life life={0};struct fake f={0};
 struct a04_sdio_direct d={&owner,&f,direct_io,0,0};struct a04_sdio_bus bus={&f,bus_io};
 struct a04_wifi_life_ops ops={&f,done,done,done,done};
 struct observation seen={0};
 uint8_t cis[]={0x20,4,0x7a,3,0x30,0x66,0xff},data[512];
 make_fixture();assert(!a04_mt6630_calibration_read(&r,&work,data));regulatory_cases(data);
 assert(!a04_nvram_sector_init(&sectors,&sr,0,readsector,8,sizeof(disk)/512,8+sizeof(disk)/512));
 assert(!a04_mt6630_calibration_read(&sr,&work,data));
 assert(!a04_wifi_admit(&owner));assert(!a04_wifi_life_begin(&life,&owner,cis,sizeof(cis),&d,&bus,&ops));
 b.life=&life;b.wlan_ready=1;b.io.ctx=&b;b.io.command=command;b.io.delay=wait_us;
 b.transport.io=&b.io;b.transport.transfer=transfer;b.transport.enabled=2;b.transport.functions=2;
 assert(!a04_wifi_life_block_verified(&life,1,512));
 b.wlan.transport=&b.transport;b.wlan.ready=b.wlan.initialized=1;
 data[4]|=1;assert(a04_wmt_bridge_load_calibration(&b,data)&&!f.reads&&!f.writes);data[4]&=~1u;
 data[196]=0;assert(a04_wmt_bridge_load_calibration(&b,data)&&!f.reads&&!f.writes);data[196]=1;
 assert(!a04_wmt_bridge_load_calibration_traced(&b,data,observe,&seen));
 assert(seen.count==3&&seen.stage[0]==A04_CP_CAL_VERSIONS&&seen.raw[0]==0);
 assert(seen.stage[1]==A04_CP_CAL_DECODE&&seen.raw[1]==0);
 assert(seen.stage[2]==A04_CP_CAL_SUBMIT&&seen.raw[2]==0&&seen.detail[2]==0x48);
 assert(b.calibration_submitted&&b.wlan.runtime&&f.reads==8&&f.writes==4);
 assert(a04_wmt_le16(f.packet)==520&&f.packet[4]==0x48&&f.packet[5]==0xa0&&f.packet[6]==1);
 assert(!memcmp(f.packet+8,data,512));for(unsigned i=520;i<1024;i++)assert(!f.packet[i]);
 for(unsigned i=8;i<520;i++)assert(!b.wlan.tx[i]);
 assert(a04_wmt_bridge_load_calibration(&b,data)&&f.writes==4);
 b.calibration_submitted=0;f.fail=1;
 memset(&seen,0,sizeof(seen));
 assert(a04_wmt_bridge_load_calibration_traced(&b,data,observe,&seen)&&b.failed&&!b.calibration_submitted&&owner.poisoned);
 assert(seen.count==3&&seen.stage[2]==A04_CP_CAL_SUBMIT&&seen.raw[2]!=0&&seen.detail[2]==0x24);
 for(unsigned i=8;i<520;i++)assert(!b.wlan.tx[i]);
 assert(a04_wmt_bridge_load_calibration(&b,data)&&f.writes==5);
 puts("NVRAM -> TX power/domain/country limit/original record; wipe and poison: OK");return 0;
}
