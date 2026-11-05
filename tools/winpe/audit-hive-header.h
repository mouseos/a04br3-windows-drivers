/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_AUDIT_HIVE_HEADER_H
#define A04_AUDIT_HIVE_HEADER_H
#include <stdint.h>
struct a04_hive_header { uint32_t primary,secondary,major,minor,checksum,computed; };
static uint32_t a04_hive_u32(const uint8_t *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int a04_hive_header(const uint8_t *p, unsigned n, struct a04_hive_header *h)
{
 unsigned i;uint32_t x=0;if(!p||!h||n<512||p[0]!='r'||p[1]!='e'||p[2]!='g'||p[3]!='f')return 0;
 h->primary=a04_hive_u32(p+4);h->secondary=a04_hive_u32(p+8);h->major=a04_hive_u32(p+0x14);h->minor=a04_hive_u32(p+0x18);h->checksum=a04_hive_u32(p+0x1fc);
 for(i=0;i<0x1fc;i+=4)x^=a04_hive_u32(p+i);if(x==0xffffffffu)x=0xfffffffeu;else if(!x)x=1;h->computed=x;
 return h->major==1&&h->minor==5&&h->checksum==h->computed;
}
#endif
