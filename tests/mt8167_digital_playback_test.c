#include <assert.h>
#include <string.h>
#include "../drivers/bus/mt8167-shared/digital_playback.h"
struct fixture { uint32_t regs[1024]; unsigned writes,fail_at; int ignore; };
static int read_reg(void *v,unsigned off,uint32_t *out) {
 struct fixture *f=v;assert(!(off&3) && off<4096);*out=f->regs[off/4];return 0;
}
static int write_reg(void *v,unsigned off,uint32_t value) {
 struct fixture *f=v;assert(!(off&3) && off<4096);
 if(++f->writes==f->fail_at)return -1;
 if(!f->ignore)f->regs[off/4]=value;
 return 0;
}
static int delay(void *v,unsigned us){(void)v;assert(us==100);return 0;}
int main(void) {
 struct fixture f={0};struct a04_digital_playback s={0};
 struct a04_digital_io io={&f,read_reg,write_reg,delay};unsigned i,total;
 f.regs[0]=(1u<<25)|(1u<<26)|0x80;
 f.regs[0xf04/4]=0xabcd1230;f.regs[0x24/4]=0x100;f.regs[0x28/4]=0x200;
 assert(a04_digital_prepare(&s,&io,44100) && !f.writes && !s.retained);
 assert(!a04_digital_prepare(&s,&io,48000) && s.prepared && s.retained && !s.poisoned);
 assert(f.regs[0]==0x80 && f.regs[0x10/4]==1);
 assert(f.regs[0x24/4]==((1u<<21)|0x100) && f.regs[0x28/4]==0x240);
 assert(f.regs[0x108/4]==0x83001803 && f.regs[0x10c/4]==0xf74f0000);
 assert(f.regs[0x124/4]==1 && f.regs[0x34/4]==0x41a09);
 assert(f.regs[0xf04/4]==0xabcd1238 && f.regs[0xf2c/4]==0x300 && f.regs[0xf28/4]==1);
 /* Digital preparation never enables ABB DL, analog DAC, or DL1 DMA. */
 assert(!f.regs[0xf00/4] && !(f.regs[0x10/4]&2));
 total=f.writes;assert(a04_digital_prepare(&s,&io,48000) && f.writes==total);
 /* Every partial write failure retains and poisons, without a retry. */
 for(i=1;i<=total;i++) {
  memset(&f,0,sizeof(f));memset(&s,0,sizeof(s));f.fail_at=i;
  assert(a04_digital_prepare(&s,&io,48000) && s.retained && s.poisoned && !s.prepared);
  assert(a04_digital_prepare(&s,&io,48000) && f.writes==i);
 }
 /* Active inherited paths are rejected before writes. */
 {unsigned offsets[]={0x10,0x108,0x124,0x114,0x34,0xf00};
  for(i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++) {
   memset(&f,0,sizeof(f));memset(&s,0,sizeof(s));f.regs[offsets[i]/4]=i?1:2;
   assert(a04_digital_prepare(&s,&io,48000) && !f.writes && !s.retained);
  }}
 memset(&f,0,sizeof(f));memset(&s,0,sizeof(s));f.regs[0]=(1u<<25);f.ignore=1;
 assert(a04_digital_prepare(&s,&io,48000) && s.poisoned && s.retained);
 /* Both latch phases must derive the command from the corresponding status. */
 memset(&f,0,sizeof(f));memset(&s,0,sizeof(s));f.regs[0xf2c/4]=0xa03;
 assert(!a04_digital_prepare(&s,&io,48000) && f.regs[0xf2c/4]==0x803);
 return 0;
}
