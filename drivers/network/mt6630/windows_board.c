/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_board.h"
static int live(void *p)
{
 struct a04_windows_wifi_board *b=p;
 return b && b->parent && b->parent->Live && b->host && b->host->base && b->owner &&
  b->owner->active && !b->owner->poisoned && b->owner->generation==b->generation &&
  b->parent->Live(b->parent->Header.Context,b->generation)==1;
}
static int same(void *p,const struct a04_wifi_owner *o,unsigned generation)
{ struct a04_windows_wifi_board *b=p;return b && b->owner==o && b->generation==generation && live(b); }
static int readreg(void *p,enum a04_wifi_board_bank bank,unsigned off,uint32_t *v)
{
 struct a04_windows_wifi_board *b=p;
 if(!live(b)||bank!=A04_WIFI_MSDC||!v||off>0xffcu||(off&3)||!b->host->pio.read)return -1;
 *v=b->host->pio.read(b->host->pio.context,off,4);return *v==UINT32_MAX?-1:0;
}
static int writereg(void *p,enum a04_wifi_board_bank bank,unsigned off,uint32_t v)
{
 struct a04_windows_wifi_board *b=p;
 if(!live(b))return -1;
 if(bank==A04_WIFI_TOP||bank==A04_WIFI_GPIO)
  return b->parent->WriteShared(b->parent->Header.Context,b->generation,(unsigned)bank,off,v);
 if(bank!=A04_WIFI_MSDC||off>0xffcu||(off&3)||!b->host->pio.write)return -1;
 b->host->pio.write(b->host->pio.context,off,v,4);return 0;
}
static int update(void *p,enum a04_wifi_board_bank bank,unsigned off,uint32_t mask,uint32_t value)
{
 struct a04_windows_wifi_board *b=p;
 if(!live(b)||(bank!=A04_WIFI_TOP&&bank!=A04_WIFI_GPIO))return -1;
 return b->parent->UpdateShared(b->parent->Header.Context,b->generation,(unsigned)bank,off,mask,value);
}
static int delay_ms(void *p,unsigned ms)
{
 struct a04_windows_wifi_board *b=p;
 if(!live(b)||ms>1000||!b->host->pio.delay)return -1;
 b->host->pio.delay(b->host->pio.context,ms*1000u);return live(b)?0:-1;
}
static void delay_us(void *p,unsigned us)
{struct a04_windows_wifi_board *b=p;b->host->pio.delay(b->host->pio.context,us);}
int a04_windows_wifi_board_init(struct a04_windows_wifi_board *b,A04_SHARED_WIFI_INTERFACE *parent,
 struct a04_windows_msdc2 *host,struct a04_wifi_owner *owner,ULONG generation)
{
 if(!b||!parent||parent->Header.Version!=A04_SHARED_WIFI_INTERFACE_VERSION||
    parent->Header.Size<sizeof(*parent)||!parent->Live||!parent->WriteShared||!parent->UpdateShared||
    !host||!host->base||!host->pio.read||!host->pio.write||!host->pio.delay||
    !owner||host->pio.owner!=owner||host->pio.generation!=generation)return -1;
 b->parent=parent;b->host=host;b->owner=owner;b->generation=generation;
 if(!live(b))return -1;
 b->io.context=b;b->io.qualified=live;b->io.read=readreg;b->io.write=writereg;
 b->io.delay_ms=delay_ms;b->io.update_shared=update;return 0;
}
int a04_windows_wifi_control_open(struct a04_windows_wifi_board *b,struct a04_wifi_board *board,
 struct a04_wifi_life *life,struct a04_wmt_bridge *wmt,uint32_t ocr,const struct a04_wifi_life_ops *ops,
 struct a04_sdio_card_info *card)
{
 int e;
 if(!b||!board||!life||!wmt||!ops||!card||!live(b))return -1;
 e=a04_wifi_bringup(board,&b->io,life,&b->host->pio,ocr,ops,card,same,b);
 if(e)return e;
 return a04_wmt_bridge_open(wmt,life,card->functions,b,delay_us);
}
