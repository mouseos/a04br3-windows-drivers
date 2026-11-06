/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SDIO_ARGS_H
#define A04_SDIO_ARGS_H
#include <stdint.h>
/* Pure command encoding, never submits hardware requests. count 1..512 bytes
 * or 1..511 blocks; zero has the SDIO byte-mode meaning of 512 bytes. */
static inline int a04_sdio_cmd52(unsigned write,unsigned fn,unsigned raw,unsigned addr,unsigned data,uint32_t *out)
{
 if(!out||write>1||raw>1||fn>7||addr>0x1ffffu||data>255||(!write&&raw))return -1;
 *out=(write<<31)|(fn<<28)|(raw<<27)|(addr<<9)|data;return 0;
}
static inline int a04_sdio_cmd53(unsigned write,unsigned fn,unsigned block,unsigned incr,unsigned addr,unsigned count,uint32_t *out)
{
 if(!out||write>1||fn>7||block>1||incr>1||addr>0x1ffffu||!count||count>(block?511u:512u))return -1;
 *out=(write<<31)|(fn<<28)|(block<<27)|(incr<<26)|(addr<<9)|(count&511u);return 0;
}
static inline int a04_sdio_r5_ok(uint32_t r5)
{ return !(r5 & ((1u<<15)|(1u<<14)|(1u<<11)|(1u<<9)|(1u<<8))); }
#endif
