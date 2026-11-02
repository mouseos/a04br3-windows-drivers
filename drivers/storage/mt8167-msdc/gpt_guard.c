/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "gpt_guard.h"
#include <stddef.h>

static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
static uint64_t le64(const uint8_t *p)
{ return (uint64_t)le32(p) | ((uint64_t)le32(p+4)<<32); }
static uint32_t crc_step(uint32_t crc, uint8_t b)
{
    unsigned i; crc ^= b;
    for (i=0;i<8;++i) crc = (crc>>1) ^ (0xedb88320u & (0u-(crc&1u)));
    return crc;
}
static int equal(const uint8_t *a, const uint8_t *b, size_t n)
{ size_t i; for (i=0;i<n;++i) if (a[i]!=b[i]) return 0; return 1; }

int a04_gpt_guard_begin(struct a04_gpt_guard *g, const uint8_t h[512], uint32_t sectors)
{
    static const uint8_t sig[8]={'E','F','I',' ','P','A','R','T'};
    uint32_t crc=0xffffffffu, n, size;
    if (!g || !h || sectors<68) return 0;
    g->valid=0; g->matches=0; g->blocks=0; g->first=g->last=0; g->range_count=0;
    size=le32(h+12);
    if (!equal(h,sig,8) || le32(h+8)!=0x10000 || size<92 || size>512 ||
        le64(h+24)!=1 || le64(h+32)!=(uint64_t)sectors-1 ||
        le64(h+40)<34 || le64(h+48)>=(uint64_t)sectors-33 ||
        le64(h+40)>le64(h+48) || le64(h+72)!=2 || le32(h+80)!=128 || le32(h+84)!=128)
        return 0;
    for (n=0;n<size;++n) crc=crc_step(crc, n>=16 && n<20 ? 0 : h[n]);
    if ((crc^0xffffffffu)!=le32(h+16)) return 0;
    g->crc=0xffffffffu; g->expected_crc=le32(h+88);
    g->usable_first=(uint32_t)le64(h+40); g->usable_last=(uint32_t)le64(h+48); g->valid=1;
    return 1;
}
int a04_gpt_guard_feed(struct a04_gpt_guard *g, const uint8_t p[512])
{
    static const uint8_t type[16]={0xa2,0xa0,0xd0,0xeb,0xe5,0xb9,0x33,0x44,0x87,0xc0,0x68,0xb6,0xb7,0x26,0x99,0xc7};
    static const uint8_t name[16]={'W',0,'i',0,'n',0,'d',0,'o',0,'w',0,'s',0,0,0};
    static const uint8_t zero[16]={0};
    unsigned i,s,j;
    if (!g || !g->valid || !p || g->blocks>=32) return 0;
    for (i=0;i<512;++i) g->crc=crc_step(g->crc,p[i]);
    for (s=0;s<4;++s) {
        const uint8_t *e=p+s*128; uint64_t first,last;
        if (equal(e,zero,16)) continue;
        first=le64(e+32); last=le64(e+40);
        if (first<g->usable_first || last<first || last>g->usable_last) {
            g->valid=0; return 0;
        }
        for (j=0;j<g->range_count;++j)
            if (first<=g->range_last[j] && g->range_first[j]<=last) { g->valid=0; return 0; }
        g->range_first[g->range_count]=(uint32_t)first;
        g->range_last[g->range_count++]=(uint32_t)last;
        if (!equal(e+56,name,16)) continue;
        for (i=16;i<72;++i) if (e[56+i]) { g->valid=0; return 0; }
        if (!equal(e,type,16)) { g->valid=0; return 0; }
        g->first=(uint32_t)first; g->last=(uint32_t)last; ++g->matches;
    }
    ++g->blocks; return 1;
}
int a04_gpt_guard_finish(const struct a04_gpt_guard *g, uint32_t *first, uint32_t *last)
{
    if (!g || !g->valid || !first || !last || g->blocks!=32 || g->matches!=1 ||
        (g->crc^0xffffffffu)!=g->expected_crc) return 0;
    *first=g->first; *last=g->last; return 1;
}
int a04_gpt_guard_range(uint32_t first, uint32_t last, uint64_t lba, uint32_t count)
{
    return count && first<=last && lba>=first && lba<=last && (uint64_t)count-1u<=last-lba;
}
