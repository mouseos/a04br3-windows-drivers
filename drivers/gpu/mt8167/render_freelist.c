/* SPDX-License-Identifier: MIT
 * Layout follows Linux pvr_free_list.c, Copyright (c) 2023 Imagination
 * Technologies Ltd., MIT option. Fixed list; firmware growth is not supported.
 */
#include "render_freelist.h"
#include "pvr_rogue_defs.h"
int a04_gpu_init_render_freelist(uint32_t *table,size_t entries,uint64_t gpu,
    const uint64_t *pages,size_t count,uint32_t id,struct rogue_fwif_freelist *out)
{
    struct rogue_fwif_freelist result={0}; size_t i;
    if (!table || !pages || !out || count<52 || count>UINT32_MAX || count>SIZE_MAX/sizeof(*table) || (count&3u) ||
        entries<count || (gpu&(ROGUE_BIF_PM_FREELIST_BASE_ADDR_ALIGNSIZE-1u)) ||
        gpu>=UINT64_C(0x10000000000) || count*sizeof(*table)>UINT64_C(0x10000000000)-gpu) return -1;
    for (i=0;i<count;i++) {
        if ((pages[i]&(ROGUE_BIF_PM_PHYSICAL_PAGE_SIZE-1u)) ||
            (pages[i]>>ROGUE_BIF_PM_PHYSICAL_PAGE_ALIGNSHIFT)>UINT32_MAX) return -1;
    }
    result.max_pages=(uint32_t)count;
    result.current_pages=(uint32_t)count;
    result.current_stack_top=(uint32_t)count-1;
    result.freelist_id=id;
    result.freelist_dev_addr=gpu;
    result.current_dev_addr=gpu;
    for (i=0;i<count;i++) table[i]=(uint32_t)(pages[i]>>ROGUE_BIF_PM_PHYSICAL_PAGE_ALIGNSHIFT);
    *out=result;
    return 0;
}
int a04_gpu_init_render_freelist_contiguous(uint32_t *table,size_t entries,uint64_t gpu,
    uint64_t first,size_t count,uint32_t id,struct rogue_fwif_freelist *out)
{
    struct rogue_fwif_freelist result={0};size_t i;
    const uint64_t limit=UINT64_C(1)<<(32u+ROGUE_BIF_PM_PHYSICAL_PAGE_ALIGNSHIFT);
    if (!table || !out || count<52 || count>UINT32_MAX || count>SIZE_MAX/sizeof(*table) || (count&3u) ||
        entries<count || !gpu || (gpu&(ROGUE_BIF_PM_FREELIST_BASE_ADDR_ALIGNSIZE-1u)) ||
        gpu>=UINT64_C(0x10000000000) || count*sizeof(*table)>UINT64_C(0x10000000000)-gpu ||
        !first || (first&(ROGUE_BIF_PM_PHYSICAL_PAGE_SIZE-1u)) || first>=limit ||
        count>(limit-first)/ROGUE_BIF_PM_PHYSICAL_PAGE_SIZE) return -1;
    result.max_pages=(uint32_t)count;result.current_pages=(uint32_t)count;
    result.current_stack_top=(uint32_t)count-1u;result.freelist_id=id;
    result.freelist_dev_addr=gpu;result.current_dev_addr=gpu;
    for (i=0;i<count;++i) table[i]=(uint32_t)((first+i*ROGUE_BIF_PM_PHYSICAL_PAGE_SIZE)>>ROGUE_BIF_PM_PHYSICAL_PAGE_ALIGNSHIFT);
    *out=result;return 0;
}
