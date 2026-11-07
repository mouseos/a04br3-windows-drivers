/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "hi556_identity.h"
static unsigned calls,position,short_read,finish_count,revoke;
static int32_t finish_error;
static int32_t begin(void *p,unsigned bus,unsigned address,uint64_t *lease)
{(void)p;assert(bus==1&&address==0x28);calls++;*lease=9;return 0;}
static int live(void *p,uint64_t lease){(void)p;assert(lease==9);return !revoke;}
static int32_t address(void *p,uint64_t lease,const uint8_t pointer[2],unsigned *done)
{(void)p;assert(lease==9&&pointer[0]==0x0f&&pointer[1]==0x16+position);calls++;*done=2;return 0;}
static int32_t read_byte(void *p,uint64_t lease,uint8_t *value,unsigned *done)
{(void)p;assert(lease==9&&position<2);calls++;*value=position?0x56:5;position++;*done=short_read?0:1;return 0;}
static int32_t finish(void *p,uint64_t lease){(void)p;assert(lease==9);calls++;finish_count++;return finish_error;}
int main(void)
{
 struct a04_hi556_io io={0,begin,live,address,read_byte,finish};struct a04_hi556_observation out;
#if !A04_HI556_IDENTITY_EXPERIMENT
 assert(a04_hi556_identify(&io,&out)==A04_HI556_DISABLED&&!calls&&!out.id_observed);
#else
 assert(a04_hi556_identify(&io,&out)==A04_HI556_OK&&out.id_observed&&out.chip_id==0x0556&&calls==6&&finish_count==1);
 position=0;short_read=1;assert(a04_hi556_identify(&io,&out)==A04_HI556_SHORT_IO&&!out.id_observed&&finish_count==2);
 position=0;short_read=0;revoke=1;assert(a04_hi556_identify(&io,&out)==A04_HI556_LEASE_LOST&&!out.id_observed&&finish_count==3);
 position=0;revoke=0;finish_error=-19;assert(a04_hi556_identify(&io,&out)==A04_HI556_FINISH_FAILED&&out.finish_status==-19);
#endif
 return 0;
}
