/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_USB_BOARD_PINS_H
#define A04_USB_BOARD_PINS_H
#include "gpio22.h"
/* MT8167 pinctrl-mtk-common: 16 direction bits/bank, five 3-bit
 * mux fields/register; a04br3.dts: 22=VBUS, 41=IDDIG, 34=USB detect.
 * Caller holds exclusive parent GPIO lock, before any source is enabled. */
static int A04UsbPinsPrepare(const A04_GPIO_IO *io)
{
 unsigned v;
 if(!io || !io->read32 || !io->write32)return -1;
 /* Keep source low before selecting its GPIO mux. */
 io->write32(io->context,0x118,1u<<6);
 v=io->read32(io->context,0x340);io->write32(io->context,0x340,v&~(7u<<6));
 if(A04Gpio22Prepare(io))return -1;
 /* MT8167 ies_set: GPIO34..39 -> 0x900 bit2; 41..43 -> bit10.
  * SET alias enables inputs without clearing any peer's enable state. */
 io->write32(io->context,0x904,(1u<<2)|(1u<<10));
 /* GPIO34 input and ID41 input/IDDIG function 2, as board pinctrl. */
 io->write32(io->context,0x28,(1u<<2)|(1u<<9));
 v=io->read32(io->context,0x360);io->write32(io->context,0x360,v&~(7u<<12));
 v=io->read32(io->context,0x380);io->write32(io->context,0x380,(v&~(7u<<3))|(2u<<3));
 return (io->read32(io->context,0x900)&((1u<<2)|(1u<<10)))!=((1u<<2)|(1u<<10)) ||
  (io->read32(io->context,0x20)&((1u<<2)|(1u<<9))) ||
  (io->read32(io->context,0x360)&(7u<<12)) ||
  (io->read32(io->context,0x380)&(7u<<3))!=(2u<<3) ? -1:0;
}
#endif
