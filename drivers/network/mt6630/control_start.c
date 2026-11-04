/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "control_start.h"
static void wait_us(void *context,unsigned us)
{ struct a04_msdc2_pio *p=context;p->delay(p->context,us); }
int a04_wifi_control_start(struct a04_wifi_life *life,struct a04_msdc2_pio *pio,
 uint32_t ocr,const struct a04_wifi_life_ops *ops,struct a04_sdio_card_info *card)
{
 struct a04_sdio_enumeration_io io;
 struct a04_sdio_direct direct;
 struct a04_sdio_bus bus;
 if(!life||life->state!=A04_WIFI_LIFE_OFF||!pio||!pio->owner||!pio->read||!pio->write||!pio->delay||!ops||!card||
    !ops->mask_and_drain_irq||!ops->drain_io||!ops->unmap_host||!ops->release_lease||
    pio->poisoned||pio->owner->poisoned||!pio->owner->active||pio->generation!=pio->owner->generation)return -1;
 io.context=pio;io.command=a04_msdc2_command;io.delay_us=wait_us;
 if(a04_mt6630_enumerate(pio->owner,&io,ocr,card))return -2;
 direct.owner=pio->owner;direct.context=pio;direct.request=a04_msdc2_cmd52;direct.generation=pio->generation;direct.poisoned=0;
 bus.context=pio;bus.request=a04_msdc2_cmd53;
 if(a04_wifi_life_begin(life,pio->owner,card->cis,card->cis_length,&direct,&bus,ops)){
  a04_wifi_fault(pio->owner);return -2;
 }
 return 0;
}
