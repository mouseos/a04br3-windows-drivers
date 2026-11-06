/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "bringup.h"
int a04_wifi_bringup(struct a04_wifi_board *board,const struct a04_wifi_board_io *io,
 struct a04_wifi_life *life,struct a04_msdc2_pio *pio,uint32_t ocr,
 const struct a04_wifi_life_ops *ops,struct a04_sdio_card_info *out,
 int (*same_lease)(void *,const struct a04_wifi_owner *,unsigned),void *context)
{
 struct a04_sdio_card_info card;
 if(!board||!io||!io->qualified||!io->read||!io->write||!io->delay_ms||!io->update_shared||
    !life||life->state!=A04_WIFI_LIFE_OFF||!pio||!pio->owner||!pio->read||!pio->write||!pio->delay||
    !ops||!ops->mask_and_drain_irq||!ops->drain_io||!ops->unmap_host||!ops->release_lease||
    !out||!same_lease||!ocr||(ocr&~0x00ff8000u)||board->started||board->ready||board->poisoned||pio->poisoned||pio->owner->poisoned||!pio->owner->active||
    pio->generation!=pio->owner->generation||same_lease(context,pio->owner,pio->generation)!=1)return -1;
 if(a04_wifi_board_start(board,io)) {a04_wifi_fault(pio->owner);return -2;}
 if(same_lease(context,pio->owner,pio->generation)!=1 ||
    a04_wifi_control_start(life,pio,ocr,ops,&card) ||
    same_lease(context,pio->owner,pio->generation)!=1) {
  board->ready=0;board->poisoned=1;
  if(life->state==A04_WIFI_LIFE_IO)life->state=A04_WIFI_LIFE_FAILED;
  if(!pio->owner->poisoned)a04_wifi_fault(pio->owner);
  return -2;
 }
 *out=card;return 0;
}
