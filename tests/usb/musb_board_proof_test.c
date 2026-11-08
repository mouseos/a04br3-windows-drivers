/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../../drivers/bus/mt8167-shared/musb_board_proof.h"
struct model {unsigned gpio[1024],mac[0x1000/4],phy[256],csr[16][2],index,writes,stuck;};
static unsigned gpio(void*p,unsigned o){return ((struct model*)p)->gpio[o/4];}
static void gw(void*p,unsigned o,unsigned v){struct model*m=p;m->writes++;if((o&15)==4)m->gpio[(o-4)/4]|=v;else if((o&15)==8)m->gpio[(o-8)/4]&=~v;else m->gpio[o/4]=v;}
static unsigned mac(void*p,unsigned o,unsigned width){struct model*m=p;(void)width;if(o==0xe)return m->index;if(o==0x12||o==0x16)return m->csr[m->index][o==0x16];return m->mac[o/4];}
static void mw(void*p,unsigned o,unsigned width,unsigned v){struct model*m=p;(void)width;m->writes++;if(o==0xe){m->index=v;return;}if(o==0x12||o==0x16){m->csr[m->index][o==0x16]=m->stuck?1:0;return;}m->mac[o/4]=v;}
static unsigned phy(void*p,unsigned o){return ((struct model*)p)->phy[o];}
static void pw(void*p,unsigned o,unsigned v){((struct model*)p)->phy[o]=v;}
static void delay(void*p,unsigned ms){(void)p;(void)ms;}
int main(void){struct model m;struct mt_musb_board_sample s;struct mt_musb_board_io i={&m,gpio,gw,mac,mw,phy,pw,delay};unsigned writes,n;
 memset(&m,0,sizeof(m));m.gpio[0x220/4]=(1<<9);m.gpio[0x210/4]=1<<5;
 assert(!mt_musb_pin_candidate(&i,&s)&&s.id&&!s.detect);writes=m.writes;assert(mt_musb_oc_prepare(&i)&&m.writes==writes); /* PC zero effects */
 m.gpio[0x220/4]=1<<2;assert(!mt_musb_oc_prepare(&i));assert(!mt_musb_pin_candidate(&i,&s)&&s.oc_valid&&s.oc&&!s.id&&s.detect);
 m.gpio[0x210/4]=0;assert(mt_musb_oc_prepare(&i));m.gpio[0x210/4]=1<<5;
 m.gpio[0x340/4]=1<<3;assert(mt_musb_pin_candidate(&i,&s));m.gpio[0x340/4]=0;
 m.phy[0x6d]=0x3e;assert(mt_musb_unforced(&i));assert(!mt_musb_release_forces(&i));
 m.mac[0x74/4]=4;assert(mt_musb_unforced(&i));m.mac[0x74/4]=0;
 m.mac[0x204/4]=1;writes=m.writes;assert(mt_musb_stop_pio(&i)&&m.writes==writes);m.mac[0x204/4]=0;
 m.mac[0xb00/4]=1;assert(mt_musb_stop_pio(&i)&&m.writes==writes);m.mac[0xb00/4]=0;
 for(n=0;n<16;n++)m.csr[n][0]=1;for(n=1;n<16;n++)m.csr[n][1]=1;
 assert(!mt_musb_stop_pio(&i));for(n=0;n<16;n++)assert(!m.csr[n][0]&&(!n||!m.csr[n][1]));
 m.stuck=1;assert(mt_musb_stop_pio(&i));
 puts("MUSB_BOARD_PC_REJECT_OC_PIN_FORCE_EXCLUSION_ALL_BANK_STOP_PASS");return 0;}
