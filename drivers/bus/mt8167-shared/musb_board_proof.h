/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_MUSB_BOARD_PROOF_H
#define MT8167_MUSB_BOARD_PROOF_H
/* Register contracts: CT3 mt8167.dtsi/a04br3.dts, pinctrl-mt8167.c,
 * mtk_musb_reg.h, musbhsdma.h, mtk_qmu.h. No assumed GPIO/ADC values. */
struct mt_musb_board_io {
 void *context;
 unsigned (*gpio)(void *,unsigned);void (*gpio_write)(void *,unsigned,unsigned);
 unsigned (*mac)(void *,unsigned,unsigned);void (*mac_write)(void *,unsigned,unsigned,unsigned);
 unsigned (*phy)(void *,unsigned);void (*phy_write)(void *,unsigned,unsigned);
 void (*delay)(void *,unsigned);
};
struct mt_musb_board_sample {unsigned id,detect,oc,source,oc_valid,band,unforced,error;};
/* Query before any pin/power edit. GPIO22/21 must already be GPIO; never
 * steal an output/mux from an unrelated function. ID41 may be GPIO input or
 * its board-described IDDIG function2. GPIO34 must be input GPIO. */
static int mt_musb_pin_candidate(const struct mt_musb_board_io *i,struct mt_musb_board_sample *s)
{
 unsigned d1=i->gpio(i->context,0x10),d2=i->gpio(i->context,0x20);
 unsigned m21=i->gpio(i->context,0x340),m34=i->gpio(i->context,0x360),m41=i->gpio(i->context,0x380);
 unsigned in2=i->gpio(i->context,0x220),in1=i->gpio(i->context,0x210);
 if(m21==~0u||m34==~0u||m41==~0u||d1==~0u||d2==~0u||in2==~0u||in1==~0u)return -1;
 if((m21&((7u<<3)|(7u<<6)))||(m34&(7u<<12))||
    ((m41&(7u<<3))!=0&&(m41&(7u<<3))!=(2u<<3))||
    (d1&(1u<<5))||(d2&((1u<<2)|(1u<<9))))return -1;
 s->id=!!(in2&(1u<<9));s->detect=!!(in2&(1u<<2));s->oc=!!(in1&(1u<<5));
 s->source=!!(i->gpio(i->context,0x110)&(1u<<6));
 s->oc_valid=!!(i->gpio(i->context,0x900)&(1u<<13));
 return 0;
}
static int mt_musb_oc_prepare(const struct mt_musb_board_io *i)
{
 struct mt_musb_board_sample s;
 if(mt_musb_pin_candidate(i,&s)||s.id||!s.detect||s.source)return -1;
 /* Shared IES21..23 is SET-only while this parent is online. It is never
  * restored/cleared while another client can use GPIO22/23. No bias edit. */
 i->gpio_write(i->context,0x904,1u<<13);
 if(!(i->gpio(i->context,0x900)&(1u<<13)))return -1;
 i->delay(i->context,1);
 if(mt_musb_pin_candidate(i,&s)||!s.oc_valid||!s.oc||s.id||!s.detect)return -1;
 i->delay(i->context,1);
 return mt_musb_pin_candidate(i,&s)||!s.oc_valid||!s.oc||s.id||!s.detect?-1:0;
}
static int mt_musb_engines_idle(const struct mt_musb_board_io *i)
{
 unsigned n;
 for(n=0;n<8;n++)if(i->mac(i->context,0x204+n*16,2)&1)return -1;
 if(i->mac(i->context,0xb00,4))return -1;
 for(n=0;n<8;n++)if((i->mac(i->context,0x810+n*16,4)|i->mac(i->context,0xa00+n*16,4))&0x8000)return -1;
 return 0;
}
static int mt_musb_unforced(const struct mt_musb_board_io *i)
{return (i->mac(i->context,0x74,1)&4)||(i->phy(i->context,0x6d)&0x3e)?-1:0;}
static int mt_musb_release_forces(const struct mt_musb_board_io *i)
{
 /* mt8167 usb20_otg_if.c programs PHY6d=1 in the actual sensing path.
  * Release the 0x3e test/forced-signal mask used by usb20_phy.c; do not
  * confuse its forced DEVCTL value with the connector comparator. */
 unsigned v=i->phy(i->context,0x6d);i->phy_write(i->context,0x6d,v&~0x3eu);
 return mt_musb_unforced(i);
}
/* MT8167 usb20_phy.c::usb_phy_recover: comparator enable alone is not a
 * sensing proof while UART/forced suspend/data/pulldown overrides remain.
 * All offsets are SIF+0x800. No source, SESSION or host-role request here. */
static int mt_musb_sensing_ready(const struct mt_musb_board_io *i)
{
 return (i->phy(i->context,0x1a)&0x90)!=0x10 ||
  (i->phy(i->context,0x6b)&4)||(i->phy(i->context,0x6e)&1)||
  (i->phy(i->context,0x68)&0xf4)||(i->phy(i->context,0x69)&0x3c)||
  (i->phy(i->context,0x6a)&0xbe)?-1:0;
}
static void mt_musb_phy_update(const struct mt_musb_board_io *i,unsigned reg,unsigned clear,unsigned set)
{unsigned v=i->phy(i->context,reg);i->phy_write(i->context,reg,(v&~clear)|set);}
static int mt_musb_prepare_sensing(const struct mt_musb_board_io *i)
{
 /* Caller has removed its source, proved engines idle and stopped PIO.
  * Same recovery fields as board_host.c; keep bus-valid overrides released. */
 mt_musb_phy_update(i,0x1d,0x10,0);mt_musb_phy_update(i,0x6b,4,0);
 mt_musb_phy_update(i,0x6e,1,0);mt_musb_phy_update(i,0x6a,4,0);
 mt_musb_phy_update(i,0x21,3,0);mt_musb_phy_update(i,0x68,0xf4,0);
 mt_musb_phy_update(i,0x69,0x3c,0);mt_musb_phy_update(i,0x6a,0xba,0);
 mt_musb_phy_update(i,0x1a,0x80,0x10);mt_musb_phy_update(i,0x18,8,6);
 /* BSP waits 800us after recovery; this PASSIVE board seam uses >=1ms. */
 i->delay(i->context,1);
 return mt_musb_sensing_ready(i)||mt_musb_unforced(i)?-1:0;
}
static int mt_musb_irq_off(const struct mt_musb_board_io *i)
{return i->mac(i->context,0xa4,4)||i->mac(i->context,6,2)||i->mac(i->context,8,2)||i->mac(i->context,0xb,1)?-1:0;}
static int mt_musb_stop_pio(const struct mt_musb_board_io *i)
{
 unsigned ep,old=i->mac(i->context,0xe,1);int failed=0;
 /* Unknown DMA/QMU is not ours to abort and cannot yield a fabric proof. */
 if(mt_musb_engines_idle(i))return -1;
 i->mac_write(i->context,0xa4,4,0);i->mac_write(i->context,6,2,0);
 i->mac_write(i->context,8,2,0);i->mac_write(i->context,0xb,1,0);
 i->mac_write(i->context,0x60,1,i->mac(i->context,0x60,1)&~1u);
 i->mac_write(i->context,1,1,i->mac(i->context,1,1)&~0xcu);
 /* Board num_eps=16. All 16 banks must be quiescent; EP0 has its own CSR. */
 for(ep=0;ep<16;ep++){
  i->mac_write(i->context,0xe,1,ep);
  if(!ep){i->mac_write(i->context,0x12,2,0x100);i->mac_write(i->context,0x12,2,0x100);if(i->mac(i->context,0x12,2)&0x23)failed=1;}
  else {
   /* Never mask an unknown endpoint's DMA enable and call it drained. */
   if((i->mac(i->context,0x12,2)&0x1000)||(i->mac(i->context,0x16,2)&0x2000)){failed=1;continue;}
   i->mac_write(i->context,0x12,2,0x2008);i->mac_write(i->context,0x12,2,0x2008);
   i->mac_write(i->context,0x16,2,0x10);i->mac_write(i->context,0x16,2,0x10);
   if((i->mac(i->context,0x12,2)&3)||(i->mac(i->context,0x16,2)&0x21))failed=1;
  }
 }
 i->mac_write(i->context,0xe,1,old);
 return failed||mt_musb_engines_idle(i)||mt_musb_irq_off(i)||(i->mac(i->context,0x60,1)&1)?-1:0;
}
#endif
