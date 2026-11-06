/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Protocol adapted from local untracked UniXNU-a04br3 A04BR3SDIOProtocol.h
 * SHA-256 52d2644220ce73d626f6d84557bf589b436d85b872b664e32206a953cafbd9b9. */
#include "enumerate.h"
#include "sdio_args.h"
#include "identity.h"
static int readb(const struct a04_sdio_enumeration_io *io,uint32_t address,uint8_t *v)
{
 uint32_t arg,r;
 if(a04_sdio_cmd52(0,0,0,address,0,&arg)||io->command(io->context,52,arg,1,&r)||!a04_sdio_r5_ok(r))return -1;
 *v=(uint8_t)r;return 0;
}
int a04_mt6630_enumerate(struct a04_wifi_owner *owner,const struct a04_sdio_enumeration_io *io,
 uint32_t requested_ocr,struct a04_sdio_card_info *out)
{
 struct a04_sdio_card_info card={0};struct a04_sdio_identity id;
 uint8_t cis[1024],v;uint32_t r,ocr,address=0;unsigned n,used=0,tuples=0,generation;
 if(!owner || !owner->active || owner->poisoned || !io || !io->command || !io->delay_us || !out ||
    !requested_ocr || (requested_ocr&~0x00ff8000u))return -1;
 generation=owner->generation;
 if(io->command(io->context,0,0,0,&r) || io->command(io->context,5,0,3,&r))goto fail;
 card.functions=(uint8_t)((r>>28)&7u);ocr=r&requested_ocr&0x00ff8000u;
 if(card.functions<2 || !ocr)goto fail;
 for(n=0;n<100;n++){
  if(io->command(io->context,5,ocr,3,&r))goto fail;
  if(r&0x80000000u)break;
  if(n!=99)io->delay_us(io->context,10000);
 }
 if(n==100 || !(r&ocr))goto fail;
 card.ocr=r;
 if(io->command(io->context,3,0,1,&r) || (r&0xe000u) || !(r&0xffff0000u))goto fail;
 card.rca=r&0xffff0000u;
 if(io->command(io->context,7,card.rca,7,&r) || (r&0xfdffa080u))goto fail;
 if(readb(io,0,&card.revision))goto fail;
 for(n=0;n<3;n++){if(readb(io,9+n,&v))goto fail;address|=(uint32_t)v<<(n*8);}
 if(!address || address>0x1ffffu)goto fail;
 while(used<sizeof(cis) && tuples++<64){
  unsigned length;
  if(address>0x1ffffu || readb(io,address++,&cis[used]))goto fail;
  v=cis[used++];if(v==0xff)break;if(!v)continue;
  if(used==sizeof(cis) || address>0x1ffffu || readb(io,address++,&cis[used]))goto fail;
  length=cis[used++];
  if(length>sizeof(cis)-used || length>0x20000u-address)goto fail;
  for(n=0;n<length;n++)if(readb(io,address++,&cis[used++]))goto fail;
 }
 if(!used || cis[used-1]!=0xff || a04_mt6630_cis(cis,used,&id) ||
    !owner->active || owner->poisoned || generation!=owner->generation)goto fail;
 card.vendor=id.vendor;card.device=id.device;card.cis_length=(uint16_t)used;
 for(n=0;n<used;n++)card.cis[n]=cis[n];
 *out=card;return 0;
fail:
 if(!owner->poisoned)a04_wifi_fault(owner);
 return -2;
}
