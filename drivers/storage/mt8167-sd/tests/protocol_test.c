/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One host protocol fixture, not proof of electrical or Windows SD operation. */
#include <assert.h>
#include <string.h>
#include "sd.h"
struct model {uint32_t regs[64],cid[4],csd[4],arg[64];unsigned commands[64],n,offset,read,write,removed_at;uint64_t now;unsigned fail_status,hang,legacy;uint8_t bytes[512];};
static void field(uint32_t words[4],unsigned start,unsigned count,uint32_t value)
{for(unsigned i=0;i<count;i++){unsigned b=start+i;words[3-b/32]&=~(1u<<(b&31));words[3-b/32]|=((value>>i)&1u)<<(b&31);}}
static uint32_t rd(void *p,unsigned off)
{
 struct model *m=p;
 if(off==0x008)return 1u<<16;
 if(off==0x000)return (m->regs[0]&~4u)|0x80u;
 if(off==0x014)return m->read?(m->offset<512?128:0):0;
 if(off==0x01c){uint32_t v=0;assert(m->read&&m->offset+4<=512);for(unsigned i=0;i<4;i++)v|=(uint32_t)m->bytes[m->offset++]<<(i*8);if(m->offset==512){m->regs[0x00c/4]|=0x1000;m->read=0;}return v;}
 return m->regs[off/4];
}
static uint8_t rd8(void *p,unsigned off){return (uint8_t)rd(p,off);}
static void wr(void *p,unsigned off,uint32_t value)
{
 struct model *m=p;
 if(off==0x00c){m->regs[off/4]&=~value;return;}
 if(off==0x014){m->offset=0;m->regs[off/4]=0;return;}
 if(off==0x018){assert(m->write&&m->offset+4<=512);for(unsigned i=0;i<4;i++)m->bytes[m->offset++]=(uint8_t)(value>>(i*8));if(m->offset==512){m->regs[0x00c/4]|=0x1000;m->write=0;}return;}
 m->regs[off/4]=value;
 if(off!=0x034)return;
 unsigned op=value&63u;assert(m->n<64);m->commands[m->n]=op;m->arg[m->n++]=m->regs[0x038/4];
 assert(op!=1&&op!=6&&op!=25); /* no eMMC init/cache/partition or multiblock write */
 if(m->hang)return;
 uint32_t response=0x900;
 if(op==8){if(m->legacy){m->regs[0x00c/4]|=0x200;return;}response=0x1aa;}
 if(op==55)response|=1u<<5;
 if(op==41)response=0x80300000u|(m->legacy?0:1u<<30);
 if(op==3)response=0x12340000;
 if(op==2||op==9)for(unsigned i=0;i<4;i++)m->regs[0x040/4+3-i]=(op==2?m->cid[i]:m->csd[i]);
 else m->regs[0x040/4]=response;
 if(op==13&&m->fail_status)m->regs[0x040/4]=1u<<19;
 if(op==17){assert(((value>>16)&4095)==512);m->offset=0;m->read=1;}
 if(op==24){assert(value&(1u<<13));assert(((value>>16)&4095)==512);m->offset=0;m->write=1;}
 m->regs[0x00c/4]|=0x100;
}
static uint64_t now(void *p){return ((struct model *)p)->now+=10;}
static void delay(void *p,unsigned us){((struct model *)p)->now+=(uint64_t)us*10;}
static int live(void *p){struct model *m=p;return !m->removed_at||m->offset<m->removed_at;}
static void model_init(struct model *m,int legacy)
{
 memset(m,0,sizeof(*m));m->legacy=(unsigned)legacy;field(m->cid,120,8,0x42);for(unsigned i=0;i<5;i++)field(m->cid,96-i*8,8,(uint32_t)('A'+i));
 field(m->csd,84,12,0x14);
 if(legacy){field(m->csd,80,4,9);field(m->csd,62,12,1023);field(m->csd,47,3,7);}
 else {field(m->csd,126,2,1);field(m->csd,48,22,0x3fff);}
}
int main(void)
{
 struct model m;struct a04_sd_card c;struct a04_sd_ops ops={{&m,rd,rd8,wr},now,delay,live};uint8_t block[512];
 model_init(&m,0);assert(a04_sd_initialize(&ops,&c,1000000)==SD_OK);assert(c.rca==0x1234&&c.sectors==0x1000000&&c.high_capacity&&c.mid==0x42&&!memcmp(c.product,"ABCDE",5));
 for(unsigned i=0;i<512;i++)m.bytes[i]=(uint8_t)i;
 assert(a04_sd_transfer(&ops,&c,123,1,0,block,1000000)==SD_OK);assert(m.arg[m.n-1]==123&&!memcmp(block,m.bytes,512));
 memset(block,0x5a,sizeof(block));assert(a04_sd_transfer(&ops,&c,456,1,1,block,1000000)==SD_OK);assert(!memcmp(block,m.bytes,512)&&m.commands[m.n-1]==13);
 c.write_protected=1;unsigned n=m.n;assert(a04_sd_transfer(&ops,&c,0,1,1,block,1000000)==SD_PROTECTED&&m.n==n);c.write_protected=0;
 m.fail_status=1;assert(a04_sd_transfer(&ops,&c,0,1,1,block,1000000)==SD_CARD&&c.poisoned);n=m.n;assert(a04_sd_transfer(&ops,&c,0,1,1,block,1000000)==SD_POISONED&&m.n==n);
 model_init(&m,0);assert(a04_sd_initialize(&ops,&c,1000000)==SD_OK);m.removed_at=64;assert(a04_sd_transfer(&ops,&c,0,1,0,block,1000000)==SD_REMOVED&&c.poisoned);
 model_init(&m,0);assert(a04_sd_initialize(&ops,&c,1000000)==SD_OK);m.hang=1;assert(a04_sd_barrier(&ops,&c,m.now+50)==SD_TIMEOUT&&c.poisoned);
 model_init(&m,1);assert(a04_sd_initialize(&ops,&c,1000000)==SD_OK);assert(!c.high_capacity&&c.sectors==524288);assert(a04_sd_transfer(&ops,&c,1,1,0,block,1000000)==SD_OK&&m.arg[m.n-1]==512);
 model_init(&m,0);field(m.csd,126,2,2);assert(a04_sd_initialize(&ops,&c,1000000)==SD_UNSUPPORTED&&!c.ready);
 return 0;
}
