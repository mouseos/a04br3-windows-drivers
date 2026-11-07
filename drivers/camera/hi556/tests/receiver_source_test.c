/* SPDX-License-Identifier: GPL-2.0-only
 * Check only the changed source RMW fields; not a hardware frame emulation. */
#include <assert.h>
#include <string.h>
#include "../capture.h"
struct fake {uint32_t r[3][0x9000/4];unsigned debug_writes;};
static uint32_t read32(void *p,unsigned r,unsigned at){return ((struct fake *)p)->r[r][at/4];}
static void write32(void *p,unsigned r,unsigned at,uint32_t value)
{
 struct fake *f=p;if(!r&&at==0x43b8)f->debug_writes++;
 if(!r&&(at==0x24||at==0x28))f->r[r][at/4]&=~value;
 else if(!r&&at==0x5c)f->r[r][at/4]=value==1?3:value;
 else f->r[r][at/4]=value;
}
static int delay(void *p,unsigned us){(void)p;(void)us;return 0;}
static int live(void *p){(void)p;return 1;}
int main(void)
{
 static struct fake f;struct a04_hi556_capture_state state={0};
 struct a04_hi556_receiver_io io={&f,read32,write32,delay,live};
 uint32_t original=0x018961e0,cal=0xabcdef10;
 f.r[0][0x43a0/4]=original;f.r[0][0x4338/4]=cal;f.r[0][0x43b8/4]=0x12345678;
 assert(!a04_hi556_receiver_arm(&io,&state,0x20000000,A04_HI556_FRAME_BYTES));
 assert(f.r[0][0x433c/4]==0x1541);
 assert(f.r[0][0x4338/4]==((cal|4u)&~1u));
 assert(f.r[0][0x43a0/4]==((original&~0x04010093u)|0x04010013u));
 assert(!f.debug_writes&&f.r[0][0x43b8/4]==0x12345678);
 assert(!a04_hi556_receiver_stop(&io,&state)&&state.quiesced);
 return 0;
}
