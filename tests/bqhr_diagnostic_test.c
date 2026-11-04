/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "bqhr_diagnostic.h"
static void put(uint8_t *p,uint64_t n){unsigned i;for(i=0;i<8;i++)p[i]=(uint8_t)(n>>(8*i));}
static void seal(uint8_t *p){uint64_t h=14695981039346656037ULL;unsigned i;for(i=0;i<120;i++){h^=p[i];h*=1099511628211ULL;}put(p+120,h);}
int main(void){uint8_t p[128]={0};struct a04_bqhr_info out;unsigned i;
 put(p,0x314f524147484334ULL);put(p+8,128);put(p+16,2);put(p+96,6);seal(p);
 assert(a04_bqhr_classify(p,128,&out)==A04_BQHR_STOPPED && out.reason==6 && !out.kicks);
 for(i=0;i<128;i++){p[i]^=1; assert(a04_bqhr_classify(p,128,&out)==A04_BQHR_INVALID);p[i]^=1;}
 assert(a04_bqhr_classify(p,127,&out)==A04_BQHR_INVALID);
 put(p+16,1);seal(p);assert(a04_bqhr_classify(p,128,&out)==A04_BQHR_OTHER_STATE);
 put(p+16,2);put(p+64,256);seal(p);assert(a04_bqhr_classify(p,128,&out)==A04_BQHR_INVALID);
 put(p+64,0);put(p+104,1);seal(p);assert(a04_bqhr_classify(p,128,&out)==A04_BQHR_INVALID);
 return 0;}
