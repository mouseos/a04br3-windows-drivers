/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "pio_transfer.h"
struct model{unsigned index;unsigned char bank[8][128],fifo[8][1024];unsigned out[8],in[8];};
static unsigned char r8(void*p,unsigned o){struct model*m=p;if(o>=0x20&&o<0x40&&!(o&3))return m->fifo[(o-0x20)/4][m->in[(o-0x20)/4]++];return m->bank[m->index][o];}
static unsigned short r16(void*p,unsigned o){struct model*m=p;return m->bank[m->index][o]|((unsigned)m->bank[m->index][o+1]<<8);}
static void w8(void*p,unsigned o,unsigned char v){struct model*m=p;if(o==0xe)m->index=v;else if(o>=0x20&&o<0x40&&!(o&3)){unsigned n=(o-0x20)/4;assert(m->out[n]<sizeof(m->fifo[n]));m->fifo[n][m->out[n]++]=v;}else if(o<128)m->bank[m->index][o]=v;}
static void w16(void*p,unsigned o,unsigned short v){struct model*m=p;if(o>=128)return;if((o==0x12&&(v&8))||(o==0x16&&(v&16)))v=0;m->bank[m->index][o]=(unsigned char)v;m->bank[m->index][o+1]=(unsigned char)(v>>8);}
int main(void){
 struct model m={0};MT8167_USB20_IO io={&m,r8,r16,w8,w16};struct a04_pio_transfer t[8];
 struct a04_pio_endpoint e={1,1,64,0x80,2,1,0,0};unsigned char data[128];unsigned n,k;
 memset(data,0xa5,sizeof(data));
 for(n=1;n<=7;n++){
  e.address=n;assert(!a04_pio_configure_lane(&io,&t[n],&e,data,128,n));
  assert(m.out[n]==64);m.index=n;
  assert(r16(&m,0x64)==MT_MUSB_FIFO_BASE(n)/8&&r16(&m,0x66)==(MT_MUSB_FIFO_BASE(n)+512)/8);
 }
 assert(MT_MUSB_FIFO_BASE(7)+1024<=8192);
 assert(a04_pio_configure_lane(&io,&t[0],&e,data,128,0)<0&&a04_pio_configure_lane(&io,&t[0],&e,data,128,8)<0);
 /* Lane1 keeps NAKing; every other resident lane progresses and completes. */
 for(k=0;k<2;k++)for(n=1;n<=7;n++){
  m.index=n;if(n==1){w16(&m,0x12,0x2082);assert(!a04_pio_event(&io,&t[n])&&!t[n].actual&&m.out[n]==64);}
  else {w16(&m,0x12,0x100);assert(a04_pio_event(&io,&t[n])==(k==1?1:0));}
 }
 for(n=2;n<=7;n++)assert(t[n].actual==128&&!memcmp(m.fifo[n],data,128));
 m.index=2;assert(!a04_pio_abort(&io,&t[1])); /* selects its own lane1 */
 assert(m.index==1&&!t[1].active&&t[2].actual==128);
 return 0;
}
