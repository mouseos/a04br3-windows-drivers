/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <string.h>
#include "../drivers/camera/hi556/capture.h"
struct fake {uint32_t r[3][0x9000/4];unsigned writes,ack;};
static uint32_t read32(void *p,unsigned r,unsigned at){struct fake *f=p;assert(r<3&&at<0x9000&&!(at&3));return f->r[r][at/4];}
static void write32(void *p,unsigned r,unsigned at,uint32_t v)
{struct fake *f=p;f->writes++;if(!r&&(at==0x24||at==0x28)){f->r[r][at/4]&=~v;return;}f->r[r][at/4]=v;if(!r&&at==0x5c&&v==1&&f->ack)f->r[r][at/4]|=2;}
static int delay(void *p,unsigned us){(void)p;assert(us<=500);return 0;}
static int live(void *p){(void)p;return 1;}
int main(void)
{
 static struct fake f;struct a04_hi556_capture_state s={0};struct a04_hi556_receiver_io o={&f,read32,write32,delay,live};
 assert(a04_hi556_receiver_arm(&o,&s,0xffffffffu,A04_HI556_FRAME_BYTES)&&!f.writes);
 f.r[2][0xfc0/4]=1;assert(a04_hi556_receiver_arm(&o,&s,0x20000000,A04_HI556_FRAME_BYTES)&&!f.writes);
 f.r[2][0xfc0/4]=0;f.ack=1;assert(!a04_hi556_receiver_arm(&o,&s,0x20000000,A04_HI556_FRAME_BYTES));
 assert(!a04_hi556_receiver_poll(&o,&s));f.r[0][0x24/4]=1u<<10;assert(!a04_hi556_receiver_poll(&o,&s));
 f.r[0][0x28/4]=1;assert(a04_hi556_receiver_poll(&o,&s)==1);
 f.r[0][0x24/4]|=1u<<20;assert(a04_hi556_receiver_poll(&o,&s)<0);
 assert(!a04_hi556_receiver_stop(&o,&s)&&s.quiesced&&!s.armed);
 f.ack=0;assert(a04_hi556_receiver_arm(&o,&s,0x20000000,A04_HI556_FRAME_BYTES)&&s.uncertain);
 return 0;
}
