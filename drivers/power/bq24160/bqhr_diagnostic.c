/* SPDX-License-Identifier: MIT */
#include "bqhr_diagnostic.h"
static uint64_t get64(const uint8_t *p)
{ uint64_t v=0; unsigned i; for(i=0;i<8;++i)v|=(uint64_t)p[i]<<(i*8); return v; }
static uint64_t seal(const uint8_t *p)
{ uint64_t h=14695981039346656037ULL; unsigned i; for(i=0;i<120;++i){h^=p[i];h*=1099511628211ULL;} return h; }
enum a04_bqhr_result a04_bqhr_classify(const uint8_t *p, unsigned size, struct a04_bqhr_info *out)
{
    struct a04_bqhr_info candidate;
    if(!p || !out || size!=128 || get64(p)!=0x314f524147484334ULL ||
       get64(p+8)!=128 || get64(p+120)!=seal(p) || get64(p+104) || get64(p+112)) return A04_BQHR_INVALID;
    candidate.state=get64(p+16); candidate.kicks=get64(p+48); candidate.faults=get64(p+56);
    candidate.first_valid=get64(p+64); candidate.latest_valid=get64(p+80); candidate.reason=get64(p+96);
    if(candidate.first_valid>255 || candidate.latest_valid>255 || candidate.faults>255 || candidate.reason>6)
        return A04_BQHR_INVALID;
    *out=candidate;
    return candidate.state==2 ? A04_BQHR_STOPPED : A04_BQHR_OTHER_STATE;
}
