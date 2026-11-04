/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "render_freelist.h"
int main(void)
{
    uint64_t pages[52]; uint32_t table[52], before[52];
    struct rogue_fwif_freelist fw, old; unsigned i;
    for (i=0;i<52;i++) pages[i]=UINT64_C(0x123400000)+(uint64_t)i*4096;
    memset(table,0xa5,sizeof(table)); memset(&fw,0x5a,sizeof(fw));
    memcpy(before,table,sizeof(table)); old=fw;
    pages[17]++;
    assert(a04_gpu_init_render_freelist(table,52,0x8000010000ULL,pages,52,7,&fw));
    assert(!memcmp(table,before,sizeof(table)) && !memcmp(&fw,&old,sizeof(fw)));
    pages[17]--;
    assert(!a04_gpu_init_render_freelist(table,52,0x8000010000ULL,pages,52,7,&fw));
    assert(fw.freelist_id==7 && fw.max_pages==52 && fw.current_pages==52 &&
           fw.current_stack_top==51 && fw.grow_pages==0 && fw.ready_pages==0 &&
           fw.freelist_dev_addr==0x8000010000ULL && fw.current_dev_addr==0x8000010000ULL);
    for (i=0;i<52;i++) assert(table[i]==(uint32_t)(pages[i]>>12));
    assert(a04_gpu_init_render_freelist(table,52,0x8000010001ULL,pages,52,7,&fw));
    assert(a04_gpu_init_render_freelist(table,51,0x8000010000ULL,pages,52,7,&fw));
    return 0;
}
