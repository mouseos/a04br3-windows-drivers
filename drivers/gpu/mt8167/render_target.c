/* SPDX-License-Identifier: MIT */
#include "render_target.h"
static int valid_freelist(const struct a04_gpu_fw_arena *a,const struct a04_gpu_fw_object *o)
{
    size_t off;
    const struct rogue_fwif_freelist *f;
    if (!a || !a->heap || !o || !o->cpu || o->size!=sizeof(*f) ||
        (o->fw_addr&0xff00003fu)!=0xc0000000u) return 0;
    off=o->fw_addr&0x00ffffffu;
    if (off>a->next || o->size>a->next-off || o->cpu!=a->heap+off || o->dma!=a->dma+off) return 0;
    f=o->cpu;
    return f->max_pages>=52 && f->current_pages==f->max_pages &&
           f->current_stack_top==f->current_pages-1 && f->freelist_dev_addr &&
           f->current_dev_addr==f->freelist_dev_addr && !f->grow_pages;
}
int a04_gpu_prepare_render_target(struct a04_gpu_fw_arena *a,
    const struct a04_gpu_fw_object *local,const struct a04_gpu_fw_object *global,
    const struct a04_render_hwrt_params *params,struct a04_gpu_render_target *out)
{
    struct a04_gpu_render_target result={0}; struct a04_render_hwrt_params p;
    size_t saved;
    if (!a || !params || !out || !valid_freelist(a,local) || !valid_freelist(a,global) ||
        local->fw_addr==global->fw_addr) return -1;
    saved=a->next;
    if (a04_gpu_fw_alloc(a,sizeof(struct rogue_fwif_hwrtdata_common),4096,&result.common) ||
        a04_gpu_fw_alloc(a,sizeof(struct rogue_fwif_hwrtdata),4096,&result.data)) { a->next=saved; return -1; }
    p=*params; p.common_fw_address=result.common.fw_addr;
    p.freelist_fw_address[0]=local->fw_addr; p.freelist_fw_address[1]=global->fw_addr;
    if (a04_gpu_init_render_hwrt(&p,result.common.cpu,result.data.cpu)) { a->next=saved; return -1; }
    *out=result; return 0;
}
