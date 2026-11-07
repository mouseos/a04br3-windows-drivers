/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One source-contract fixture, not DMA/IRQ/hardware proof. */
#include "../hfp_memif.h"
#include <assert.h>
#include <string.h>
struct model {uint32_t r[1024];unsigned writes;};
static int rd(void *p,unsigned off,uint32_t *v){struct model *m=p;assert(!(off&3)&&off<4096);*v=m->r[off/4];return 0;}
static int wr(void *p,unsigned off,uint32_t v){struct model *m=p;assert(!(off&3)&&off<4096);m->writes++;if(off==0x3a8)m->r[0x3a4/4]&=~v;else m->r[off/4]=v;return 0;}
int main(void)
{
 struct model hw={0};struct mt8167_hfp_memif m={0};
 struct mt8167_hfp_io io={&hw,rd,wr};
 const struct mt8167_hfp_ring fallback[2]={{0x400000,4096,1},{0x500000,4096,2}};
 const struct mt8167_hfp_ring client={0x600000,8192,3};uint32_t position,status;
 hw.r[0x14/4]=0xa0000000;hw.r[0x3d8/4]=0x40000000;
 assert(!mt8167_hfp_memif_prepare(&m,&io,fallback));
 assert((hw.r[0x14/4]&0x0020000f)==0x00200000);assert(hw.r[0x14/4]&0xa0000000);
 assert(!(hw.r[0x10/4]&0x03000000));assert(!(hw.r[0x3d8/4]&0x03030000));
 assert(!mt8167_hfp_memif_start(&m));assert(m.exposed&&(hw.r[0x10/4]&0x13)==0x13);
 assert(!mt8167_hfp_memif_bind(&m,0,&client));
 hw.r[0x44/4]=client.base+256;assert(!mt8167_hfp_memif_position(&m,0,3,&position)&&position==256);
 assert(mt8167_hfp_memif_position(&m,0,2,&position));
 hw.r[0x44/4]=client.base+1;assert(mt8167_hfp_memif_position(&m,0,3,&position));
 hw.r[0x3a4/4]=0x43;assert(!mt8167_hfp_memif_irq(&m,&status)&&status==3&&hw.r[0x3a4/4]==0x40);
 assert(!mt8167_hfp_memif_inactive(&m,0));assert(hw.r[0x40/4]==fallback[0].base);
 assert(mt8167_hfp_memif_position(&m,0,3,&position));
 assert(!mt8167_hfp_memif_stop_registers(&m));
 assert(mt8167_hfp_memif_restore(&m)); /* Stop register writes are NOT bus drain. */
 memset(&m,0,sizeof(m));hw.r[0x10/4]=2;unsigned writes=hw.writes;
 assert(mt8167_hfp_memif_prepare(&m,&io,fallback)&&hw.writes==writes);
 return 0;
}
