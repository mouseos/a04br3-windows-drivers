/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "windows_board.h"
static int online=1,updates,writes,msdc,wmt_calls;
static int live(PVOID p,ULONG g){(void)p;return online&&g==7;}
static int update(PVOID p,ULONG g,unsigned b,unsigned o,ULONG m,ULONG v)
{(void)p;assert(g==7&&b==1&&o==0x300&&m==(7u<<12)&&v==0);updates++;return 0;}
static int write_shared(PVOID p,ULONG g,unsigned b,unsigned o,ULONG v)
{(void)p;assert(g==7&&b==0&&o==0xa0&&v==2);writes++;return 0;}
static uint32_t readpio(void *p,unsigned off,unsigned w){(void)p;assert(off==0&&w==4);return 0x80;}
static void writepio(void *p,unsigned off,uint32_t v,unsigned w){(void)p;assert(off==0x10&&v==0&&w==4);msdc++;}
static void delay(void *p,unsigned us){(void)p;assert(us<=1000000);}
int a04_wifi_bringup(struct a04_wifi_board *board,const struct a04_wifi_board_io *io,
 struct a04_wifi_life *life,struct a04_msdc2_pio *pio,uint32_t ocr,const struct a04_wifi_life_ops *ops,
 struct a04_sdio_card_info *card,int (*same)(void *,const struct a04_wifi_owner *,unsigned),void *ctx)
{
 uint32_t value;(void)board;(void)life;(void)ops;assert(ocr==0x100000&&same(ctx,pio->owner,7));
 assert(!io->update_shared(io->context,A04_WIFI_GPIO,0x300,7u<<12,0));
 assert(!io->write(io->context,A04_WIFI_TOP,0xa0,2));
 assert(!io->read(io->context,A04_WIFI_MSDC,0,&value)&&value==0x80);
 assert(!io->write(io->context,A04_WIFI_MSDC,0x10,0));
 assert(io->read(io->context,A04_WIFI_GPIO,0,&value));
 assert(!io->delay_ms(io->context,30));card->functions=2;return 0;
}
int a04_wmt_bridge_open(struct a04_wmt_bridge *w,struct a04_wifi_life *l,unsigned fn,void *c,void (*d)(void *,unsigned))
{(void)w;(void)l;assert(fn==2);d(c,10);wmt_calls++;return 0;}
int main(void)
{
 A04_SHARED_WIFI_INTERFACE iface={0};struct a04_windows_msdc2 host={0};struct a04_wifi_owner owner={0};
 struct a04_windows_wifi_board b={0};struct a04_wifi_board board={0};struct a04_wifi_life life={0};
 struct a04_wifi_life_ops ops={0};struct a04_sdio_card_info card={0};static struct a04_wmt_bridge wmt;
 iface.Header.Version=A04_SHARED_WIFI_INTERFACE_VERSION;iface.Header.Size=sizeof(iface);
 iface.Live=live;iface.UpdateShared=update;iface.WriteShared=write_shared;
 owner.active=1;owner.generation=7;host.base=(void *)1;host.pio.owner=&owner;host.pio.generation=7;
 host.pio.read=readpio;host.pio.write=writepio;host.pio.delay=delay;
 assert(!a04_windows_wifi_board_init(&b,&iface,&host,&owner,7));
 assert(!a04_windows_wifi_control_open(&b,&board,&life,&wmt,0x100000,&ops,&card));
 assert(updates==1&&writes==1&&msdc==1&&wmt_calls==1);
 online=0;assert(a04_windows_wifi_control_open(&b,&board,&life,&wmt,0x100000,&ops,&card));
 assert(updates==1&&writes==1&&msdc==1&&wmt_calls==1);
 puts("Windows parent board and WMT control adapter: OK");return 0;
}
