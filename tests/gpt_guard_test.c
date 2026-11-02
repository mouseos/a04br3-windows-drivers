/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <stdint.h>
#include <stdio.h>
#include "gpt_guard.h"
int main(int argc, char **argv)
{
    struct a04_gpt_guard g; uint8_t block[512]; uint32_t first,last; unsigned i;
    FILE *f;
    if (argc!=2 || !(f=fopen(argv[1],"rb"))) return 2;
    if (fread(block,1,512,f)!=512 || !a04_gpt_guard_begin(&g,block,4096)) return 1;
    for (i=0;i<32;++i)
        if (fread(block,1,512,f)!=512 || !a04_gpt_guard_feed(&g,block)) return 1;
    fclose(f);
    if (!a04_gpt_guard_finish(&g,&first,&last) || first!=512 || last!=2047) return 1;
    if (!a04_gpt_guard_range(first,last,512,1) || !a04_gpt_guard_range(first,last,2040,8) ||
        a04_gpt_guard_range(first,last,511,1) || a04_gpt_guard_range(first,last,2047,2) ||
        a04_gpt_guard_range(first,last,UINT64_MAX,2) || a04_gpt_guard_range(first,last,512,0)) return 1;
    return 0;
}
