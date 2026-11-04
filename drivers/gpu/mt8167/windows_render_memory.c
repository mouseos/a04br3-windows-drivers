/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_render_memory.h"
#if A04_GE8300_RENDER_EXPERIMENT
#include "gpu_mmu.h"
#include "render_pds_data.h"
#include "render_freelist.h"
#include "pvr_rogue_fwif.h"
#define PAGE 4096u
#define TABLE_PAGES 14u
#define MAX_PAYLOAD_PAGES 4096u
#define FRAGMENT_USC_BYTES 360u
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<=b ? b-a<n : a-b<m; }
static int owned_fw(const struct a04_windows_render_memory *m,const struct a04_gpu_fw_object *o,size_t size)
{
    size_t off;
    if (!m->firmware || !m->firmware->memory.heap || !o || o->size!=size ||
        m->firmware->runtime.next_free>A04_PVR_HEAP_SIZE || (o->fw_addr&0xff00003fu)!=0xc0000000u) return 0;
    off=(size_t)(o->fw_addr&0x00ffffffu);
    return off<=m->firmware->runtime.next_free && size<=m->firmware->runtime.next_free-off &&
        o->cpu==(uint8_t *)m->firmware->memory.heap+off && o->dma==(uint64_t)m->firmware->memory.heap_logical.QuadPart+off;
}
static int writable_range(const struct a04_windows_render_memory *m,uint64_t addr,uint64_t len)
{
    size_t i;unsigned matches=0;
    if (!addr || !len || addr>=UINT64_C(1)<<40 || len>(UINT64_C(1)<<40)-addr) return 0;
    for (i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];uint64_t n=(uint64_t)r->pages*PAGE;
        if (!(r->mmu_flags&A04_GPU_MMU_READ_ONLY) && addr>=r->gpu_va && len<=n && addr-r->gpu_va<=n-len) ++matches;
    }
    return matches==1;
}
static int freelists_valid(const struct a04_windows_render_memory *m,uint64_t callstack)
{
    const struct a04_windows_render_freelist_layout *l=m->freelist_layout;
    size_t i,j,payload;uint64_t dma;
    if (m->length<A04_RENDER_PAYLOAD_OFFSET || !m->map_count || m->map_count>A04_RENDER_MAX_MAPS) return 0;
    payload=(m->length-A04_RENDER_PAYLOAD_OFFSET)/PAGE;
    if (!l[0].id || !l[1].id || l[0].id==l[1].id ||
        overlap(l[0].first_payload_page,l[0].page_count,l[1].first_payload_page,l[1].page_count) ||
        overlap(l[0].table_gpu,(uint64_t)l[0].page_count*4u,l[1].table_gpu,(uint64_t)l[1].page_count*4u)) return 0;
    for (i=0;i<2;++i) {
        const struct rogue_fwif_freelist *f=m->freelists[i].cpu;const uint32_t *table=0;unsigned matches=0;
        uint64_t bytes=(uint64_t)l[i].page_count*4u;
        if (!f || m->freelists[i].size!=sizeof(*f) || l[i].page_count<52u || (l[i].page_count&15u) ||
            l[i].first_payload_page>=payload || l[i].page_count>payload-l[i].first_payload_page ||
            !l[i].table_gpu || (l[i].table_gpu&63u) || overlap(l[i].table_gpu,bytes,callstack,8) ||
            f->freelist_id!=l[i].id || f->max_pages!=l[i].page_count || f->current_pages!=l[i].page_count ||
            f->current_stack_top!=l[i].page_count-1u || f->grow_pages || f->ready_pages || f->grow_pending ||
            f->allocated_page_count || f->allocated_mmu_page_count || f->freelist_flags || f->pm_global_pb ||
            f->freelist_dev_addr!=l[i].table_gpu || f->current_dev_addr!=l[i].table_gpu) return 0;
        for (j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t n=(uint64_t)r->pages*PAGE;
            if (overlap(l[i].first_payload_page,l[i].page_count,r->payload_page,r->pages)) return 0;
            if (!(r->mmu_flags&A04_GPU_MMU_READ_ONLY) && (r->mmu_flags&A04_GPU_MMU_SLC_BYPASS) && l[i].table_gpu>=r->gpu_va && bytes<=n && l[i].table_gpu-r->gpu_va<=n-bytes) {
                uint64_t off=(uint64_t)A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+(l[i].table_gpu-r->gpu_va);
                if (off>m->length || bytes>m->length-off) return 0;
                table=(const uint32_t *)(const void *)((const uint8_t *)m->cpu+off);++matches;
            }
        }
        if (matches!=1) return 0;
        dma=(uint64_t)m->logical.QuadPart+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)l[i].first_payload_page*PAGE;
        for (j=0;j<l[i].page_count;++j) if (table[j]!=(uint32_t)((dma+(uint64_t)j*PAGE)>>12)) return 0;
    }
    return 1;
}
NTSTATUS a04_windows_render_memory_create(struct a04_windows_render_memory *m,
    struct a04_gpu_firmware_state *s,ULONG payload_pages,
    const struct a04_windows_render_map *maps,size_t count)
{
    uint64_t dma; uint8_t *p; size_t i,j; ULONG total;
    if (!m || !s || !maps || !count || count>A04_RENDER_MAX_MAPS ||
        !payload_pages || payload_pages>MAX_PAYLOAD_PAGES || m->cpu || m->adapter || m->firmware ||
        !s->processor_released || !s->firmware_started || s->queue_faulted ||
        !s->memory.heap || !s->memory.adapter || !s->memory.adapter->DmaOperations ||
        !s->memory.adapter->DmaOperations->AllocateCommonBuffer ||
        !s->memory.adapter->DmaOperations->FreeCommonBuffer || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    for (i=0;i<count;++i) {
        const struct a04_windows_render_map *a=&maps[i];
        if (!a->pages || a->group>=4 || a->payload_page>=payload_pages ||
            a->pages>payload_pages-a->payload_page || (a->gpu_va&(PAGE-1)) ||
            !a->gpu_va || a->gpu_va>=UINT64_C(1)<<40 ||
            (uint64_t)a->pages*PAGE>(UINT64_C(1)<<40)-a->gpu_va ||
            (a->mmu_flags & ~(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_COHERENT|A04_GPU_MMU_SLC_BYPASS)))
            return STATUS_INVALID_PARAMETER;
        for (j=0;j<i;++j) {
            const struct a04_windows_render_map *b=&maps[j];
            if (overlap(a->gpu_va,(uint64_t)a->pages*PAGE,b->gpu_va,(uint64_t)b->pages*PAGE) ||
                overlap(a->payload_page,a->pages,b->payload_page,b->pages)) return STATUS_INVALID_PARAMETER;
        }
    }
    total=(TABLE_PAGES+payload_pages)*PAGE;
    m->firmware=s;m->adapter=s->memory.adapter;m->length=total;
    m->cpu=m->adapter->DmaOperations->AllocateCommonBuffer(m->adapter,total,&m->logical,FALSE);
    if (!m->cpu) { m->firmware=0;m->adapter=0;m->length=0;return STATUS_INSUFFICIENT_RESOURCES; }
    dma=(uint64_t)m->logical.QuadPart;p=m->cpu;
    if (((uintptr_t)p&(PAGE-1)) || (dma&(PAGE-1)) || dma>UINT64_C(0x100000000)-total ||
        overlap(dma,total,(uint64_t)s->memory.heap_logical.QuadPart,A04_PVR_HEAP_SIZE) ||
        (s->memory.table && overlap(dma,total,(uint64_t)s->memory.table_logical.QuadPart,0x4000u)))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    RtlZeroMemory(p,total);
    if (a04_gpu_mmu_map_brn65273((uint32_t *)(void *)p,(uint64_t *)(void *)(p+PAGE),
        (uint64_t *)(void *)(p+2u*PAGE),(uint64_t *)(void *)(p+3u*PAGE),
        (uint64_t *)(void *)(p+4u*PAGE),dma+PAGE,dma+2u*PAGE,dma+3u*PAGE,dma+4u*PAGE,dma+5u*PAGE))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    for (i=0;i<count;++i) {
        const struct a04_windows_render_map *r=&maps[i];size_t pd=6u+2u*r->group,pt=pd+1u;
        if (a04_gpu_mmu_map_4k((uint32_t *)(void *)p,(uint64_t *)(void *)(p+pd*PAGE),
            (uint64_t *)(void *)(p+pt*PAGE),dma+pd*PAGE,dma+pt*PAGE,r->gpu_va,
            dma+(TABLE_PAGES+r->payload_page)*PAGE,r->pages,r->mmu_flags))
            return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    for (i=0;i<count;++i) m->maps[i]=maps[i];
    m->map_count=count;m->ready=TRUE;
    return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_memory_bind_vm(struct a04_windows_render_memory *m)
{
    struct a04_gpu_fw_arena arena={0};struct a04_gpu_fw_object vm={0};
    struct a04_gpu_firmware_state *s;size_t first;
    if (!m || !m->ready || !m->cpu || !m->firmware || !m->adapter || m->published || m->poisoned ||
        m->vm.cpu || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    s=m->firmware;
    if (!s->processor_released || !s->firmware_started || s->queue_faulted || !s->memory.heap ||
        s->memory.adapter!=m->adapter || s->runtime.next_free>A04_PVR_HEAP_SIZE-PAGE)
        return STATUS_INVALID_DEVICE_STATE;
    first=(s->runtime.next_free+PAGE-1u)&~(size_t)(PAGE-1u);
    if (a04_gpu_fw_arena_init(&arena,s->memory.heap,A04_PVR_HEAP_SIZE,
        (uint64_t)s->memory.heap_logical.QuadPart,first) ||
        a04_gpu_fw_memory_context(&arena,(uint64_t)m->logical.QuadPart,&vm)) return STATUS_INSUFFICIENT_RESOURCES;
    s->runtime.next_free=(uint32_t)arena.next;m->vm=vm;
    return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_prepare_context(struct a04_windows_render_memory *m,
    uint32_t context_id,uint64_t callstack,unsigned isp_count,
    const struct rogue_fwif_geom_registers_caswitch *static_state)
{
    struct a04_gpu_fw_arena arena={0};struct a04_gpu_render_context ctx={0};
    struct a04_gpu_firmware_state *s;size_t first,i;int owned=0;
    if (!m || !m->ready || !m->cpu || !m->vm.cpu || !m->firmware || !m->adapter ||
        m->published || m->poisoned || m->render_prepared || !static_state || !context_id ||
        !callstack || (callstack&15u) || !m->map_count || m->map_count>A04_RENDER_MAX_MAPS ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    s=m->firmware;
    if (!s->processor_released || !s->firmware_started || s->queue_faulted || !s->memory.heap ||
        s->memory.adapter!=m->adapter || s->runtime.next_free>A04_PVR_HEAP_SIZE-PAGE)
        return STATUS_INVALID_DEVICE_STATE;
    for (i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];uint64_t n=(uint64_t)r->pages*PAGE;
        if (!(r->mmu_flags&A04_GPU_MMU_READ_ONLY) && callstack>=r->gpu_va && n>=8u && callstack-r->gpu_va<=n-8u) ++owned;
    }
    if (owned!=1) return STATUS_INVALID_PARAMETER;
    first=(s->runtime.next_free+PAGE-1u)&~(size_t)(PAGE-1u);
    if (a04_gpu_fw_arena_init(&arena,s->memory.heap,A04_PVR_HEAP_SIZE,
        (uint64_t)s->memory.heap_logical.QuadPart,first) ||
        a04_gpu_prepare_render_context(&arena,&m->vm,context_id,callstack,isp_count,static_state,&ctx))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    s->runtime.next_free=(uint32_t)arena.next;m->render=ctx;m->render_prepared=TRUE;
    return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_prepare_freelists(struct a04_windows_render_memory *m,
    const struct a04_windows_render_freelist_layout layouts[2])
{
    struct a04_windows_render_freelist_layout l[2];struct a04_gpu_fw_object obj[2]={{0},{0}};
    struct a04_gpu_fw_arena arena={0};struct a04_gpu_firmware_state *s;
    uint32_t *tables[2]={0,0};uint64_t dma[2];size_t i,j,first,payload_pages;
    if (!m || !layouts || !m->ready || !m->cpu || !m->firmware || !m->adapter || !m->render_prepared ||
        m->published || m->poisoned || m->freelists_prepared || !m->map_count || m->map_count>A04_RENDER_MAX_MAPS ||
        m->length<A04_RENDER_PAYLOAD_OFFSET || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    l[0]=layouts[0];l[1]=layouts[1];s=m->firmware;payload_pages=(m->length-A04_RENDER_PAYLOAD_OFFSET)/PAGE;
    if (!s->processor_released || !s->firmware_started || s->queue_faulted || s->memory.adapter!=m->adapter ||
        !s->memory.heap || s->runtime.next_free>A04_PVR_HEAP_SIZE-PAGE || !l[0].id || !l[1].id || l[0].id==l[1].id)
        return STATUS_INVALID_DEVICE_STATE;
    for (i=0;i<2;++i) {
        uint64_t bytes=(uint64_t)l[i].page_count*4u;unsigned matches=0;
        if (l[i].page_count<52u || (l[i].page_count&15u) || l[i].first_payload_page>=payload_pages ||
            l[i].page_count>payload_pages-l[i].first_payload_page || !l[i].table_gpu || (l[i].table_gpu&63u))
            return STATUS_INVALID_PARAMETER;
        for (j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t n=(uint64_t)r->pages*PAGE;
            if (overlap(l[i].first_payload_page,l[i].page_count,r->payload_page,r->pages)) return STATUS_INVALID_PARAMETER;
            if (!(r->mmu_flags&A04_GPU_MMU_READ_ONLY) && (r->mmu_flags&A04_GPU_MMU_SLC_BYPASS) && l[i].table_gpu>=r->gpu_va && bytes<=n && l[i].table_gpu-r->gpu_va<=n-bytes) {
                uint64_t off=(uint64_t)A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+(l[i].table_gpu-r->gpu_va);
                if (off>m->length || bytes>m->length-off) return STATUS_INVALID_PARAMETER;
                tables[i]=(uint32_t *)(void *)((uint8_t *)m->cpu+off);++matches;
            }
        }
        if (matches!=1) return STATUS_INVALID_PARAMETER;
        dma[i]=(uint64_t)m->logical.QuadPart+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)l[i].first_payload_page*PAGE;
    }
    if (overlap(l[0].first_payload_page,l[0].page_count,l[1].first_payload_page,l[1].page_count) ||
        overlap(l[0].table_gpu,(uint64_t)l[0].page_count*4u,l[1].table_gpu,(uint64_t)l[1].page_count*4u)) return STATUS_INVALID_PARAMETER;
    if (!owned_fw(m,&m->render.geom_state,sizeof(struct rogue_fwif_geom_ctx_state))) return STATUS_INVALID_DEVICE_STATE;
    { const struct rogue_fwif_geom_ctx_state *geom=m->render.geom_state.cpu;
      uint64_t cs=geom->geom_core[0].geom_reg_vdm_call_stack_pointer_init;
      if (overlap(l[0].table_gpu,(uint64_t)l[0].page_count*4u,cs,8) ||
          overlap(l[1].table_gpu,(uint64_t)l[1].page_count*4u,cs,8)) return STATUS_INVALID_PARAMETER; }
    first=(s->runtime.next_free+PAGE-1u)&~(size_t)(PAGE-1u);
    if (a04_gpu_fw_arena_init(&arena,s->memory.heap,A04_PVR_HEAP_SIZE,(uint64_t)s->memory.heap_logical.QuadPart,first) ||
        a04_gpu_fw_alloc(&arena,sizeof(struct rogue_fwif_freelist),4096,&obj[0]) ||
        a04_gpu_fw_alloc(&arena,sizeof(struct rogue_fwif_freelist),4096,&obj[1]) ||
        a04_gpu_init_render_freelist_contiguous(tables[0],l[0].page_count,l[0].table_gpu,dma[0],l[0].page_count,l[0].id,obj[0].cpu) ||
        a04_gpu_init_render_freelist_contiguous(tables[1],l[1].page_count,l[1].table_gpu,dma[1],l[1].page_count,l[1].id,obj[1].cpu))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    s->runtime.next_free=(uint32_t)arena.next;
    for (i=0;i<2;++i) { m->freelists[i]=obj[i];m->freelist_layout[i]=l[i]; }
    m->freelists_prepared=TRUE;return STATUS_SUCCESS;
}
static int target_ranges_valid(const struct a04_windows_render_memory *m,const struct a04_render_hwrt_params *p)
{
    uint64_t addr[7],len[7],pages,pt,pd,pc,mlist,cs;size_t i,j;
    const struct rogue_fwif_geom_ctx_state *geom=m->render.geom_state.cpu;
    if (!p || !geom || !owned_fw(m,&m->render.geom_state,sizeof(*geom)) || !m->freelists_prepared ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || p->width!=64 || p->height!=64 ||
        p->tpc_stride || p->tpc_size!=128 || p->region_header_size!=16 || p->rtc_gpu || p->macrotile_gpu || (p->pm_mlist_gpu&4095u) ||
        p->merge_lower_x || p->merge_lower_y || p->merge_upper_x || p->merge_upper_y || p->merge_scale_x || p->merge_scale_y)
        return 0;
    /* Mesa pvr_rt_get_mlist_size: page-table entries required by the two
     * freelist table byte sizes, two PM address spaces and 4-byte entries. */
    pages=((uint64_t)m->freelist_layout[0].page_count*4u+(uint64_t)m->freelist_layout[1].page_count*4u)>>12;
    if (!pages) return 0;
    pt=(pages+511u)/512u;pd=(pt+511u)/512u;pc=(pd+1023u)/1024u;
    mlist=((pt+pd+pc)*8u+4095u)&~UINT64_C(4095);
    cs=geom->geom_core[0].geom_reg_vdm_call_stack_pointer_init;
    addr[0]=p->tpc_gpu;len[0]=128;addr[1]=p->vheap_gpu;len[1]=1536;
    addr[2]=p->pm_mlist_gpu;len[2]=mlist;addr[3]=p->region_gpu;len[3]=128;
    addr[4]=m->freelist_layout[0].table_gpu;len[4]=(uint64_t)m->freelist_layout[0].page_count*4u;
    addr[5]=m->freelist_layout[1].table_gpu;len[5]=(uint64_t)m->freelist_layout[1].page_count*4u;
    addr[6]=cs;len[6]=8;
    for(i=0;i<7;++i) {
        if (!writable_range(m,addr[i],len[i])) return 0;
        for(j=0;j<i;++j) if (overlap(addr[i],len[i],addr[j],len[j])) return 0;
    }
    return 1;
}
NTSTATUS a04_windows_render_prepare_target(struct a04_windows_render_memory *m,
    const struct a04_render_hwrt_params *params)
{
    struct a04_gpu_fw_arena arena={0};struct a04_gpu_render_target target={0};
    struct a04_render_hwrt_params p;struct a04_gpu_firmware_state *s;size_t first;
    if (!m || !params || !m->ready || !m->render_prepared || !m->freelists_prepared || !m->firmware ||
        m->target_prepared || m->published || m->poisoned || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    p=*params;s=m->firmware;
    if (!s->processor_released || !s->firmware_started || s->queue_faulted || !s->memory.heap ||
        s->memory.adapter!=m->adapter || s->runtime.next_free>A04_PVR_HEAP_SIZE-PAGE ||
        !owned_fw(m,&m->render.geom_state,sizeof(struct rogue_fwif_geom_ctx_state)) ||
        !owned_fw(m,&m->freelists[0],sizeof(struct rogue_fwif_freelist)) ||
        !owned_fw(m,&m->freelists[1],sizeof(struct rogue_fwif_freelist)) ||
        !freelists_valid(m,((struct rogue_fwif_geom_ctx_state *)m->render.geom_state.cpu)->geom_core[0].geom_reg_vdm_call_stack_pointer_init) ||
        !target_ranges_valid(m,&p)) return STATUS_INVALID_DEVICE_STATE;
    first=(s->runtime.next_free+PAGE-1u)&~(size_t)(PAGE-1u);
    if (a04_gpu_fw_arena_init(&arena,s->memory.heap,A04_PVR_HEAP_SIZE,(uint64_t)s->memory.heap_logical.QuadPart,first) ||
        a04_gpu_prepare_render_target(&arena,&m->freelists[0],&m->freelists[1],&p,&target)) return STATUS_DEVICE_CONFIGURATION_ERROR;
    s->runtime.next_free=(uint32_t)arena.next;m->target=target;m->target_params=p;m->target_prepared=TRUE;
    return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_fragment_pds(struct a04_windows_render_memory *m,
    uint64_t pds_gpu,uint64_t shader_gpu,uint64_t doutu_control)
{
    const struct a04_windows_render_map *pds=0,*shader=0;
    struct a04_render_owned_range range;uint8_t data[48];size_t i;uint64_t off,shader_off;
    if (!m || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (pds_gpu&15u) ||
        (pds_gpu>>32)!=UINT64_C(0xda) || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    for (i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *map=&m->maps[i];uint64_t n=(uint64_t)map->pages*PAGE;
        if (pds_gpu>=map->gpu_va && n>=sizeof(data) && pds_gpu-map->gpu_va<=n-sizeof(data)) pds=map;
        if (shader_gpu>=map->gpu_va && n>=FRAGMENT_USC_BYTES && shader_gpu-map->gpu_va<=n-FRAGMENT_USC_BYTES) shader=map;
    }
    if (!pds || !shader || !(pds->mmu_flags&A04_GPU_MMU_READ_ONLY) ||
        !(shader->mmu_flags&A04_GPU_MMU_READ_ONLY) || overlap(pds_gpu,sizeof(data),shader_gpu,FRAGMENT_USC_BYTES))
        return STATUS_INVALID_PARAMETER;
    off=(uint64_t)A04_RENDER_PAYLOAD_OFFSET+(uint64_t)pds->payload_page*PAGE+(pds_gpu-pds->gpu_va);
    shader_off=(uint64_t)A04_RENDER_PAYLOAD_OFFSET+(uint64_t)shader->payload_page*PAGE;
    if (off>m->length || sizeof(data)>m->length-off || shader_off>=m->length ||
        (uint64_t)shader->pages*PAGE>m->length-shader_off) return STATUS_INVALID_PARAMETER;
    range.gpu_va=shader->gpu_va;range.dma=(uint64_t)m->logical.QuadPart+shader_off;
    range.size=(uint64_t)shader->pages*PAGE;range.access=A04_RENDER_RANGE_READ|A04_RENDER_RANGE_EXEC;
    if (a04_render_make_fragment_pds(shader_gpu,doutu_control,&range,1,data)) return STATUS_INVALID_PARAMETER;
    for (i=0;i<sizeof(data);++i) ((uint8_t *)m->cpu)[off+i]=data[i];
    return STATUS_SUCCESS;
}
static NTSTATUS render_memory_publish(struct a04_windows_render_memory *m,BOOLEAN context_synced)
{
    NTSTATUS status;size_t off;struct rogue_fwif_fwmemcontext *vm;
    if (!m || !m->firmware || !m->cpu || !m->adapter || !m->ready || !m->vm.cpu || m->published || m->poisoned ||
        (m->render_prepared && !context_synced) ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    off=(size_t)(m->vm.fw_addr&0x00ffffffu);
    if (!m->firmware->processor_released || !m->firmware->firmware_started || m->firmware->queue_faulted ||
        !m->firmware->memory.heap || m->firmware->memory.adapter!=m->adapter ||
        m->firmware->runtime.next_free>A04_PVR_HEAP_SIZE ||
        (m->vm.fw_addr&0xff00003fu)!=0xc0000000u || m->vm.size!=sizeof(*vm) ||
        off>m->firmware->runtime.next_free || m->vm.size>m->firmware->runtime.next_free-off ||
        m->vm.cpu!=(uint8_t *)m->firmware->memory.heap+off ||
        m->vm.dma!=(uint64_t)m->firmware->memory.heap_logical.QuadPart+off) return STATUS_INVALID_DEVICE_STATE;
    vm=m->vm.cpu;
    if (vm->pc_dev_paddr!=(uint64_t)m->logical.QuadPart || vm->page_cat_base_reg_set!=ROGUE_FW_BIF_INVALID_PCSET)
        return STATUS_INVALID_DEVICE_STATE;
    m->poisoned=TRUE;
    status=a04_gpu_dma_sync(m->cpu,m->length,FALSE);
    if (!NT_SUCCESS(status)) return status;
    status=a04_gpu_dma_sync(m->vm.cpu,(ULONG)m->vm.size,FALSE);
    if (!NT_SUCCESS(status)) return status;
    status=a04_gpu_firmware_invalidate_mmu(m->firmware);
    if (!NT_SUCCESS(status)) return status;
    m->published=TRUE;m->poisoned=FALSE;return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_memory_publish(struct a04_windows_render_memory *m)
{
    return render_memory_publish(m,FALSE);
}
NTSTATUS a04_windows_render_publish_context(struct a04_windows_render_memory *m)
{
    struct a04_gpu_fw_object *objects[13];struct rogue_fwif_fwrendercontext *ctx;
    struct rogue_fwif_geom_ctx_state *geom;uint64_t callstack;int owned=0;size_t i,j,count=9;NTSTATUS status;
    if (!m || !m->render_prepared || !m->ready || !m->firmware || !m->cpu || !m->vm.cpu ||
        m->published || m->poisoned || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    objects[0]=&m->render.context;objects[1]=&m->render.geom_control;objects[2]=&m->render.geom_ring;
    objects[3]=&m->render.geom_state;objects[4]=&m->render.geom_fence;objects[5]=&m->render.frag_control;
    objects[6]=&m->render.frag_ring;objects[7]=&m->render.frag_state;objects[8]=&m->render.frag_fence;
    if (m->freelists_prepared) { objects[9]=&m->freelists[0];objects[10]=&m->freelists[1];count=11; }
    if (m->target_prepared) {
        if (!m->freelists_prepared) return STATUS_INVALID_DEVICE_STATE;
        objects[11]=&m->target.common;objects[12]=&m->target.data;count=13;
    }
    for (i=0;i<count;++i) {
        struct a04_gpu_fw_object *o=objects[i];size_t off=(size_t)(o->fw_addr&0x00ffffffu);
        if (!m->firmware->memory.heap || m->firmware->runtime.next_free>A04_PVR_HEAP_SIZE ||
            (o->fw_addr&0xff00003fu)!=0xc0000000u || !o->size || o->size>UINT32_MAX ||
            off>m->firmware->runtime.next_free || o->size>m->firmware->runtime.next_free-off ||
            o->cpu!=(uint8_t *)m->firmware->memory.heap+off ||
            o->dma!=(uint64_t)m->firmware->memory.heap_logical.QuadPart+off) return STATUS_INVALID_DEVICE_STATE;
        for (j=0;j<i;++j) if (overlap(o->dma,o->size,objects[j]->dma,objects[j]->size)) return STATUS_INVALID_DEVICE_STATE;
    }
    if (m->render.context.size!=sizeof(*ctx) || m->render.geom_ring.size!=(1u<<15) ||
        m->render.frag_ring.size!=(1u<<15) || m->render.geom_control.size!=sizeof(struct rogue_fwif_cccb_ctl) ||
        m->render.frag_control.size!=sizeof(struct rogue_fwif_cccb_ctl) ||
        m->render.geom_state.size!=sizeof(*geom) || m->render.geom_fence.size!=sizeof(uint32_t) ||
        m->render.frag_fence.size!=sizeof(uint32_t) || m->render.frag_state.size<sizeof(struct rogue_fwif_frag_ctx_state)+4u ||
        m->render.frag_state.size>sizeof(struct rogue_fwif_frag_ctx_state)+64u*4u ||
        (m->render.frag_state.size-sizeof(struct rogue_fwif_frag_ctx_state))%4u) return STATUS_INVALID_DEVICE_STATE;
    ctx=m->render.context.cpu;
    if (ctx->geom_context.dm!=PVR_FWIF_DM_GEOM || ctx->frag_context.dm!=PVR_FWIF_DM_FRAG ||
        ctx->geom_context.fw_mem_context_fw_addr!=m->vm.fw_addr || ctx->frag_context.fw_mem_context_fw_addr!=m->vm.fw_addr ||
        ctx->geom_context.ccb_fw_addr!=m->render.geom_ring.fw_addr || ctx->frag_context.ccb_fw_addr!=m->render.frag_ring.fw_addr ||
        ctx->geom_context.ccbctl_fw_addr!=m->render.geom_control.fw_addr || ctx->frag_context.ccbctl_fw_addr!=m->render.frag_control.fw_addr ||
        ctx->geom_context.context_state_addr!=m->render.geom_state.fw_addr || ctx->frag_context.context_state_addr!=m->render.frag_state.fw_addr ||
        m->render.geom_context_fw!=m->render.context.fw_addr+offsetof(struct rogue_fwif_fwrendercontext,geom_context) ||
        m->render.frag_context_fw!=m->render.context.fw_addr+offsetof(struct rogue_fwif_fwrendercontext,frag_context)) return STATUS_INVALID_DEVICE_STATE;
    geom=m->render.geom_state.cpu;callstack=geom->geom_core[0].geom_reg_vdm_call_stack_pointer_init;
    if (!callstack || (callstack&15u) || !m->map_count || m->map_count>A04_RENDER_MAX_MAPS) return STATUS_INVALID_DEVICE_STATE;
    for (i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];uint64_t n=(uint64_t)r->pages*PAGE;
        if (!(r->mmu_flags&A04_GPU_MMU_READ_ONLY) && callstack>=r->gpu_va && n>=8u && callstack-r->gpu_va<=n-8u) ++owned;
    }
    if (owned!=1) return STATUS_INVALID_DEVICE_STATE;
    if (m->freelists_prepared && !freelists_valid(m,callstack)) return STATUS_INVALID_DEVICE_STATE;
    if (m->target_prepared) {
        const struct rogue_fwif_hwrtdata *d=m->target.data.cpu;
        const struct rogue_fwif_hwrtdata_common *c=m->target.common.cpu;
        if (m->target.data.size!=sizeof(*d) || m->target.common.size!=sizeof(*c) ||
            !target_ranges_valid(m,&m->target_params) || d->hwrt_data_common_fw_addr!=m->target.common.fw_addr ||
            d->freelists_fw_addr[0]!=m->freelists[0].fw_addr || d->freelists_fw_addr[1]!=m->freelists[1].fw_addr ||
            d->tail_ptrs_dev_addr!=m->target_params.tpc_gpu || d->vheap_table_dev_addr!=m->target_params.vheap_gpu ||
            d->pm_mlist_dev_addr!=m->target_params.pm_mlist_gpu || d->rgn_header_dev_addr!=m->target_params.region_gpu ||
            d->rtc_dev_addr || d->macrotile_array_dev_addr || d->hwrt_data_flags || d->state || d->rta_ctl.max_rts!=1 ||
            c->tpc_size!=128 || c->tpc_stride || c->rgn_header_size!=16 || c->screen_pixel_max!=((63u<<16)|63u) ||
            c->multi_sample_ctl!=0x88u || c->flipped_multi_sample_ctl!=0x88u || c->te_screen!=((3u<<12)|3u) ||
            c->mtile_stride!=16 || c->teaa || c->te_mtile1!=(4u<<18) || c->te_mtile2!=(4u<<18) ||
            c->isp_mtile_size!=((4u<<16)|4u) || c->isp_merge_lower_x || c->isp_merge_lower_y ||
            c->isp_merge_upper_x || c->isp_merge_upper_y || c->isp_merge_scale_x || c->isp_merge_scale_y)
            return STATUS_INVALID_DEVICE_STATE;
    }
    /* Mesa's freelist BOs require PM/FW-protected PTEs. The current Windows
     * page-table encoder has no such flag, so never expose these lists to
     * firmware until that mapping contract is implemented and verified. */
    if (m->freelists_prepared) return STATUS_NOT_SUPPORTED;
    for (i=0;i<count;++i) {
        status=a04_gpu_dma_sync(objects[i]->cpu,(ULONG)objects[i]->size,FALSE);
        if (!NT_SUCCESS(status)) { m->poisoned=TRUE;return status; }
    }
    return render_memory_publish(m,TRUE);
}
NTSTATUS a04_windows_render_memory_free(struct a04_windows_render_memory *m, BOOLEAN gpu_dma_quiesced)
{
    if (!m || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    if (!m->firmware && !m->adapter && !m->cpu) return STATUS_SUCCESS;
    if (!m->firmware || !m->adapter || !m->cpu || !m->length) return STATUS_INVALID_DEVICE_STATE;
    if (m->firmware->processor_released || !gpu_dma_quiesced) return STATUS_DEVICE_BUSY;
    m->adapter->DmaOperations->FreeCommonBuffer(m->adapter,m->length,m->logical,m->cpu,FALSE);
    RtlZeroMemory(m,sizeof(*m));return STATUS_SUCCESS;
}
#endif
