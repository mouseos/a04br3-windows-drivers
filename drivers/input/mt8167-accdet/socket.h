/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_ACCDET_SOCKET_H
#define MT8167_ACCDET_SOCKET_H
#include <stdint.h>
/* GPIO19 external jack switch only; no PMIC accessory/microphone classification.
 * Facts: CT3 a04br3.dts, mt8167 pinctrl, accdet_eint_func. */
struct mt8167_accdet_socket {uint32_t size,version,qualified,inserted;};
struct mt8167_accdet_io {void *context;int (*read32)(void *,unsigned,uint32_t *);};
enum mt8167_accdet_result {MT8167_ACCDET_OK,MT8167_ACCDET_INVALID,MT8167_ACCDET_IO,MT8167_ACCDET_NOT_READY,MT8167_ACCDET_CHANGED};
static inline enum mt8167_accdet_result mt8167_accdet_read_socket(const struct mt8167_accdet_io *io,struct mt8167_accdet_socket *s)
{
 uint32_t dir,mode,ies,pull,select,first,last,dir2,mode2,ies2,pull2,select2;
 if(!s)return MT8167_ACCDET_INVALID;
 s->size=sizeof(*s);s->version=1;s->qualified=0;s->inserted=0;
 if(!io||!io->read32)return MT8167_ACCDET_INVALID;
 if(io->read32(io->context,0x010,&dir)||io->read32(io->context,0x330,&mode)||
    io->read32(io->context,0x910,&ies)||io->read32(io->context,0x510,&pull)||
    io->read32(io->context,0x610,&select)||io->read32(io->context,0x210,&first)||
    io->read32(io->context,0x010,&dir2)||io->read32(io->context,0x330,&mode2)||
    io->read32(io->context,0x910,&ies2)||io->read32(io->context,0x510,&pull2)||
    io->read32(io->context,0x610,&select2)||io->read32(io->context,0x210,&last))return MT8167_ACCDET_IO;
 /* Board-source qualified bias tuple: stock LK disables BOTH pin19 bias
  * bits, and Linux accdet inherits those while requesting mode0/input/IES.
  * Keep legacy verified internal pull-up supported. A lone changed bias bit
  * is NOT a fallback. Parent additionally requires its D0 GPIO19 lease. */
 if((dir&(1u<<3))||(mode&(7u<<12))||!(ies&(1u<<10))||(!!(pull&(1u<<3))!=!!(select&(1u<<3))))return MT8167_ACCDET_NOT_READY;
 if(((dir^dir2)&(1u<<3))||((mode^mode2)&(7u<<12))||((ies^ies2)&(1u<<10))||
    ((pull^pull2)&(1u<<3))||((select^select2)&(1u<<3))||((first^last)&(1u<<3)))return MT8167_ACCDET_CHANGED;
 s->qualified=1;s->inserted=(last&(1u<<3))?0u:1u;return MT8167_ACCDET_OK;
}
#endif
