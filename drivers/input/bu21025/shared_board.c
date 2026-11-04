/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "shared_board.h"
static enum a04_shared_bank bank(enum a04_touch_register_bank b)
{ return b==A04_TOUCH_GPIO?A04_SHARED_GPIO:b==A04_TOUCH_INFRA?A04_SHARED_INFRA:A04_SHARED_TOP; }
static int update(void *p,enum a04_touch_register_bank b,unsigned off,uint32_t mask,uint32_t bits)
{ struct a04_touch_shared_board *s=p;if(b!=A04_TOUCH_GPIO&&b!=A04_TOUCH_INFRA&&b!=A04_TOUCH_TOP)return -1;return a04_register_touch_update(s->owner,bank(b),off,mask,bits); }
static int write_reg(void *p,enum a04_touch_register_bank b,unsigned off,uint32_t v)
{ struct a04_touch_shared_board *s=p;if(b!=A04_TOUCH_GPIO&&b!=A04_TOUCH_INFRA&&b!=A04_TOUCH_TOP)return -1;return a04_register_touch_write(s->owner,bank(b),off,v); }
static int i2c(void *p,unsigned off,uint16_t v)
{ struct a04_touch_shared_board *s=p;return s->i2c_write16(s->i2c_context,off,v); }
int a04_touch_shared_bind(struct a04_touch_shared_board *s,struct a04_touch_board_io *out)
{
    if(!s||!s->owner||!s->i2c_write16||!out)return -1;
    out->context=s;out->update32=update;out->write32=write_reg;out->write_i2c16=i2c;return 0;
}
int a04_touch_shared_pen(struct a04_touch_shared_board *s,int *down)
{ return !s ? -1 : a04_register_touch_input(s->owner,down); }
