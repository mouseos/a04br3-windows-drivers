/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/input/mt8167-accdet/socket.h"
struct fixture {uint32_t dir,mode,ies,pull,select,din;unsigned calls,fail,change;};
static int read32(void *p,unsigned off,uint32_t *v)
{
 struct fixture *f=p;unsigned call=++f->calls;
 if(f->fail==call)return -1;
 switch(off){case 0x10:*v=f->dir;break;case 0x330:*v=f->mode;break;case 0x910:*v=f->ies;break;
 case 0x510:*v=f->pull;break;case 0x610:*v=f->select;break;case 0x210:*v=f->din;break;default:assert(0);}
 if(f->change==call)*v^=off==0x910?1u<<10:off==0x330?1u<<12:1u<<3;
 return 0;
}
static struct fixture good(void){struct fixture f={0};f.ies=1u<<10;f.pull=f.select=f.din=1u<<3;return f;}
int main(void){
 struct fixture f=good();struct mt8167_accdet_io io={&f,read32};struct mt8167_accdet_socket s;
 assert(mt8167_accdet_read_socket(&io,&s)==MT8167_ACCDET_OK&&s.size==16&&s.version==1&&s.qualified&&!s.inserted);
 f=good();f.din=0;assert(mt8167_accdet_read_socket(&io,&s)==MT8167_ACCDET_OK&&s.inserted);
 for(unsigned i=1;i<=12;i++){f=good();f.fail=i;assert(mt8167_accdet_read_socket(&io,&s)==MT8167_ACCDET_IO&&!s.qualified&&!s.inserted);}
 for(unsigned i=7;i<=12;i++){f=good();f.change=i;assert(mt8167_accdet_read_socket(&io,&s)==MT8167_ACCDET_CHANGED&&!s.qualified&&!s.inserted);}
 for(unsigned i=0;i<5;i++){f=good();if(i==0)f.dir=1u<<3;if(i==1)f.mode=1u<<12;if(i==2)f.ies=0;if(i==3)f.pull=0;if(i==4)f.select=0;
  assert(mt8167_accdet_read_socket(&io,&s)==MT8167_ACCDET_NOT_READY&&!s.qualified&&!s.inserted);}
 f=good();f.dir=~(1u<<3);f.mode=~(7u<<12);f.ies=f.pull=f.select=f.din=~0u;
 assert(mt8167_accdet_read_socket(&io,&s)==MT8167_ACCDET_OK&&!s.inserted); /* unrelated pins ignored */
 assert(mt8167_accdet_read_socket(0,&s)==MT8167_ACCDET_INVALID&&!s.qualified);
 return 0;
}
