/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_render_memory.h"
#if A04_GE8300_RENDER_EXPERIMENT
#include "gpu_mmu.h"
#include "render_pds_data.h"
#include "render_context_pds.h"
#include "render_context_bounded_usc.h"
#include "render_shared_size.h"
#include "render_freelist.h"
#include "render_pair.h"
#include "pvr_rogue_fwif.h"
#include "pvr_rogue_fwif_client.h"
#define PAGE 4096u
#define TABLE_PAGES 14u
#define MAX_PAYLOAD_PAGES 4096u
#define FRAGMENT_USC_BYTES 360u
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<=b ? b-a<n : a-b<m; }
static int bytes_equal(const void *a,const void *b,size_t n)
{ size_t i;for(i=0;i<n;++i) if (((const uint8_t *)a)[i]!=((const uint8_t *)b)[i]) return 0;return 1; }
/* Reservations survive until the allocation is retired. Zero bytes alone do
 * not imply a destination is unused: padding can be part of a live program. */
static int staged_available(const struct a04_windows_render_memory *m,
    const uint64_t *addr,const uint64_t *len,size_t n)
{
    size_t i,j;
    if (m->staged_count>A04_RENDER_MAX_STAGED || m->writable_count>A04_RENDER_MAX_WRITABLE ||
        n>A04_RENDER_MAX_STAGED-m->staged_count) return 0;
    for(i=0;i<n;++i) {
        if (!len[i] || addr[i]>=UINT64_C(1)<<40 || len[i]>(UINT64_C(1)<<40)-addr[i]) return 0;
        for(j=0;j<i;++j) if(overlap(addr[i],len[i],addr[j],len[j])) return 0;
        for(j=0;j<m->staged_count;++j) if(overlap(addr[i],len[i],m->staged[j].gpu_va,m->staged[j].size)) return 0;
        for(j=0;j<m->writable_count;++j) if(overlap(addr[i],len[i],m->writable[j].gpu_va,m->writable[j].size)) return 0;
    }
    return 1;
}
static int writable_available(const struct a04_windows_render_memory *m,
    const uint64_t *addr,const uint64_t *len,size_t n)
{
    size_t i,j;
    if (m->staged_count>A04_RENDER_MAX_STAGED || m->writable_count>A04_RENDER_MAX_WRITABLE ||
        n>A04_RENDER_MAX_WRITABLE-m->writable_count) return 0;
    for(i=0;i<n;++i) {
        if (!addr[i] || !len[i] || addr[i]>=UINT64_C(1)<<40 || len[i]>(UINT64_C(1)<<40)-addr[i]) return 0;
        for(j=0;j<i;++j) if(overlap(addr[i],len[i],addr[j],len[j])) return 0;
        for(j=0;j<m->staged_count;++j) if(overlap(addr[i],len[i],m->staged[j].gpu_va,m->staged[j].size)) return 0;
        for(j=0;j<m->writable_count;++j) if(overlap(addr[i],len[i],m->writable[j].gpu_va,m->writable[j].size)) return 0;
    }
    return 1;
}
static void writable_commit(struct a04_windows_render_memory *m,const uint64_t *addr,const uint64_t *len,size_t n)
{
    size_t i;for(i=0;i<n;++i) { m->writable[m->writable_count].gpu_va=addr[i];m->writable[m->writable_count].size=len[i];++m->writable_count; }
}
static void staged_commit(struct a04_windows_render_memory *m,const uint64_t *addr,const uint64_t *len,size_t n)
{
    size_t i;for(i=0;i<n;++i) { m->staged[m->staged_count].gpu_va=addr[i];m->staged[m->staged_count].size=len[i];++m->staged_count; }
}
static int owned_fw(const struct a04_windows_render_memory *m,const struct a04_gpu_fw_object *o,size_t size)
{
    size_t off;
    if (!m->firmware || !m->firmware->memory.heap || !o || o->size!=size ||
        m->firmware->runtime.next_free>A04_PVR_HEAP_SIZE || (o->fw_addr&0xff00003fu)!=0xc0000000u) return 0;
    off=(size_t)(o->fw_addr&0x00ffffffu);
    return off<=m->firmware->runtime.next_free && size<=m->firmware->runtime.next_free-off &&
        o->cpu==(uint8_t *)m->firmware->memory.heap+off && o->dma==(uint64_t)m->firmware->memory.heap_logical.QuadPart+off;
}
static int writable_range(const struct a04_windows_render_memory *m,uint64_t addr,uint64_t len,int pm_only)
{
    size_t i;unsigned matches=0;
    if (!addr || !len || addr>=UINT64_C(1)<<40 || len>(UINT64_C(1)<<40)-addr) return 0;
    for (i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];uint64_t n=(uint64_t)r->pages*PAGE;
        if (!(r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !!(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT)==!!pm_only &&
            addr>=r->gpu_va && len<=n && addr-r->gpu_va<=n-len) ++matches;
    }
    return matches==1;
}
static int reservations_valid(const struct a04_windows_render_memory *m)
{
    size_t i,j;
    if (m->staged_count>A04_RENDER_MAX_STAGED || m->writable_count>A04_RENDER_MAX_WRITABLE) return 0;
    for(i=0;i<m->writable_count;++i) {
        uint64_t a=m->writable[i].gpu_va,n=m->writable[i].size;
        if (!writable_range(m,a,n,0) && !writable_range(m,a,n,1)) return 0;
        for(j=0;j<i;++j) if(overlap(a,n,m->writable[j].gpu_va,m->writable[j].size)) return 0;
        for(j=0;j<m->staged_count;++j) if(overlap(a,n,m->staged[j].gpu_va,m->staged[j].size)) return 0;
    }
    return 1;
}
/* Match the actual DMA page tables, not just the saved mapping description. */
static int render_ptes_valid(const struct a04_windows_render_memory *m)
{
    const uint8_t *p=m->cpu;const uint32_t *pc=(const uint32_t *)(const void *)p;
    uint64_t dma=(uint64_t)m->logical.QuadPart;size_t i,j,g;
    if (!p || ((uintptr_t)p&(PAGE-1u)) || (dma&(PAGE-1u)) || !m->map_count ||
        m->map_count>A04_RENDER_MAX_MAPS || m->length<A04_RENDER_PAYLOAD_OFFSET || dma>UINT64_C(0x100000000)-m->length) return 0;
    for (i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];const uint64_t *pd,*pt;
        uint64_t pd_dma,pt_dma,base;size_t ci,di,ti;
        if (r->group>=4 || !r->pages || (r->gpu_va&(PAGE-1u)) || r->gpu_va>=UINT64_C(1)<<40 ||
            (uint64_t)r->pages*PAGE>(UINT64_C(1)<<40)-r->gpu_va ||
            r->payload_page>=(m->length-A04_RENDER_PAYLOAD_OFFSET)/PAGE ||
            r->pages>(m->length-A04_RENDER_PAYLOAD_OFFSET)/PAGE-r->payload_page ||
            (r->mmu_flags&~(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_COHERENT|A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_PM_FW_PROTECT))) return 0;
        ci=(size_t)(r->gpu_va>>30);di=(size_t)((r->gpu_va>>21)&511u);ti=(size_t)((r->gpu_va>>12)&511u);
        if (r->pages>512u-ti) return 0;
        pd_dma=dma+(6u+2u*r->group)*PAGE;pt_dma=pd_dma+PAGE;
        pd=(const uint64_t *)(const void *)(p+(6u+2u*r->group)*PAGE);pt=pd+512;
        if (pc[ci]!=((uint32_t)(pd_dma>>8)|1u) || pd[di]!=(pt_dma|1u)) return 0;
        base=dma+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        for (j=0;j<r->pages;++j) if (pt[ti+j]!=((base+(uint64_t)j*PAGE)|r->mmu_flags|1u)) return 0;
        for (j=0;j<i;++j) {
            const struct a04_windows_render_map *q=&m->maps[j];
            if (overlap(q->gpu_va,(uint64_t)q->pages*PAGE,r->gpu_va,(uint64_t)r->pages*PAGE) ||
                overlap(q->payload_page,q->pages,r->payload_page,r->pages) ||
                (q->group==r->group && (q->gpu_va>>21)!=(r->gpu_va>>21)) ||
                (q->group!=r->group && (q->gpu_va>>30)==(r->gpu_va>>30))) return 0;
        }
    }
    /* A shared leaf table must not create unlisted aliases in another 2 MiB
     * region; no unlisted PTEs or page-directory links may be present. */
    for (g=0;g<4;++g) {
        const uint64_t *pd=(const uint64_t *)(const void *)(p+(6u+2u*g)*PAGE),*pt=pd+512;
        size_t di=512;uint64_t pt_dma=dma+(7u+2u*g)*PAGE;
        for (i=0;i<m->map_count;++i) if (m->maps[i].group==g) di=(size_t)((m->maps[i].gpu_va>>21)&511u);
        for (j=0;j<512;++j) {
            int listed=0;
            if (pd[j]!=(j==di ? (pt_dma|1u) : 0u)) return 0;
            for (i=0;i<m->map_count;++i) if (m->maps[i].group==g) {
                size_t ti=(size_t)((m->maps[i].gpu_va>>12)&511u);
                if (j>=ti && j-ti<m->maps[i].pages) ++listed;
            }
            if (listed>1 || (!listed && pt[j])) return 0;
        }
    }
    for (j=0;j<1024;++j) {
        uint32_t expected=0;unsigned found=0;
        if (j==992u) { expected=(uint32_t)((dma+PAGE)>>8)|1u;found=1; }
        if (j==996u) { expected=(uint32_t)((dma+3u*PAGE)>>8)|1u;found=1; }
        for (i=0;i<m->map_count;++i) if ((m->maps[i].gpu_va>>30)==j) {
            uint32_t entry=(uint32_t)((dma+(6u+2u*m->maps[i].group)*PAGE)>>8)|1u;
            if (found && expected!=entry) return 0;
            expected=entry;found=1;
        }
        if (pc[j]!=expected) return 0;
    }
    for (g=0;g<2;++g) {
        const uint64_t *pd=(const uint64_t *)(const void *)(p+(1u+2u*g)*PAGE),*pt=pd+512;
        uint64_t pt_dma=dma+(2u+2u*g)*PAGE,zero=dma+5u*PAGE;
        for (j=0;j<512;++j) if (pd[j]!=(j ? 0u : (pt_dma|1u)) || pt[j]!=(j ? 0u : (zero|1u))) return 0;
    }
    for (j=0;j<PAGE;++j) if (p[5u*PAGE+j]) return 0;
    return 1;
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
            if (!(r->mmu_flags&A04_GPU_MMU_READ_ONLY) && (r->mmu_flags&(A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_PM_FW_PROTECT))==(A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_PM_FW_PROTECT) && l[i].table_gpu>=r->gpu_va && bytes<=n && l[i].table_gpu-r->gpu_va<=n-bytes) {
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
            (a->mmu_flags & ~(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_COHERENT|A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_PM_FW_PROTECT)))
            return STATUS_INVALID_PARAMETER;
        for (j=0;j<i;++j) {
            const struct a04_windows_render_map *b=&maps[j];
            if (overlap(a->gpu_va,(uint64_t)a->pages*PAGE,b->gpu_va,(uint64_t)b->pages*PAGE) ||
                overlap(a->payload_page,a->pages,b->payload_page,b->pages) ||
                (a->group==b->group && (a->gpu_va>>21)!=(b->gpu_va>>21)) ||
                (a->group!=b->group && (a->gpu_va>>30)==(b->gpu_va>>30))) return STATUS_INVALID_PARAMETER;
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
    struct a04_gpu_firmware_state *s;size_t first,i;int owned=0;uint64_t callstack_len=8;
    if (!m || !m->ready || !m->cpu || !m->vm.cpu || !m->firmware || !m->adapter ||
        m->published || m->poisoned || m->render_prepared || !static_state || !context_id ||
        !callstack || (callstack&15u) || !m->map_count || m->map_count>A04_RENDER_MAX_MAPS ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    if (m->bounded_static_prepared && !bytes_equal(static_state,&m->bounded_static_state,sizeof(*static_state)))
        return STATUS_INVALID_PARAMETER;
    s=m->firmware;
    if (!s->processor_released || !s->firmware_started || s->queue_faulted || !s->memory.heap ||
        s->memory.adapter!=m->adapter || s->runtime.next_free>A04_PVR_HEAP_SIZE-PAGE)
        return STATUS_INVALID_DEVICE_STATE;
    for (i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];uint64_t n=(uint64_t)r->pages*PAGE;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT)) && callstack>=r->gpu_va && n>=8u && callstack-r->gpu_va<=n-8u) ++owned;
    }
    if (owned!=1 || !writable_available(m,&callstack,&callstack_len,1)) return STATUS_INVALID_PARAMETER;
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
            if (!(r->mmu_flags&A04_GPU_MMU_READ_ONLY) && (r->mmu_flags&(A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_PM_FW_PROTECT))==(A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_PM_FW_PROTECT) && l[i].table_gpu>=r->gpu_va && bytes<=n && l[i].table_gpu-r->gpu_va<=n-bytes) {
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
static int target_ranges_valid(const struct a04_windows_render_memory *m,const struct a04_render_hwrt_params *p,
    uint64_t *out_addr,uint64_t *out_len)
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
        if (!writable_range(m,addr[i],len[i],i==4 || i==5)) return 0;
        for(j=0;j<i;++j) if (overlap(addr[i],len[i],addr[j],len[j])) return 0;
    }
    if ((out_addr && !out_len) || (!out_addr && out_len)) return 0;
    if (out_addr) for(i=0;i<7;++i) { out_addr[i]=addr[i];out_len[i]=len[i]; }
    return 1;
}
NTSTATUS a04_windows_render_prepare_target(struct a04_windows_render_memory *m,
    const struct a04_render_hwrt_params *params)
{
    struct a04_gpu_fw_arena arena={0};struct a04_gpu_render_target target={0};
    struct a04_render_hwrt_params p;struct a04_gpu_firmware_state *s;size_t first;uint64_t addr[7],len[7];
    if (!m || !params || !m->ready || !m->render_prepared || !m->freelists_prepared || !m->firmware ||
        m->target_prepared || m->published || m->poisoned || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    p=*params;s=m->firmware;
    if (!s->processor_released || !s->firmware_started || s->queue_faulted || !s->memory.heap ||
        s->memory.adapter!=m->adapter || s->runtime.next_free>A04_PVR_HEAP_SIZE-PAGE ||
        !owned_fw(m,&m->render.geom_state,sizeof(struct rogue_fwif_geom_ctx_state)) ||
        !owned_fw(m,&m->freelists[0],sizeof(struct rogue_fwif_freelist)) ||
        !owned_fw(m,&m->freelists[1],sizeof(struct rogue_fwif_freelist)) ||
        !freelists_valid(m,((struct rogue_fwif_geom_ctx_state *)m->render.geom_state.cpu)->geom_core[0].geom_reg_vdm_call_stack_pointer_init) ||
        !target_ranges_valid(m,&p,addr,len) || !writable_available(m,addr,len,7)) return STATUS_INVALID_DEVICE_STATE;
    first=(s->runtime.next_free+PAGE-1u)&~(size_t)(PAGE-1u);
    if (a04_gpu_fw_arena_init(&arena,s->memory.heap,A04_PVR_HEAP_SIZE,(uint64_t)s->memory.heap_logical.QuadPart,first) ||
        a04_gpu_prepare_render_target(&arena,&m->freelists[0],&m->freelists[1],&p,&target)) return STATUS_DEVICE_CONFIGURATION_ERROR;
    s->runtime.next_free=(uint32_t)arena.next;m->target=target;m->target_params=p;m->target_prepared=TRUE;
    writable_commit(m,addr,len,7);
    return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_fragment_pds(struct a04_windows_render_memory *m,
    uint64_t pds_gpu,uint64_t shader_gpu,uint64_t doutu_control)
{
    const struct a04_windows_render_map *pds=0,*shader=0;
    struct a04_render_owned_range range;uint8_t data[48];size_t i;uint64_t off,shader_off,sz=48;
    if (!m || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (pds_gpu&15u) ||
        (pds_gpu>>32)!=UINT64_C(0xda) || KeGetCurrentIrql()!=PASSIVE_LEVEL || !render_ptes_valid(m))
        return STATUS_INVALID_PARAMETER;
    for (i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *map=&m->maps[i];uint64_t n=(uint64_t)map->pages*PAGE;
        if (pds_gpu>=map->gpu_va && n>=sizeof(data) && pds_gpu-map->gpu_va<=n-sizeof(data)) pds=map;
        if (shader_gpu>=map->gpu_va && n>=FRAGMENT_USC_BYTES && shader_gpu-map->gpu_va<=n-FRAGMENT_USC_BYTES) shader=map;
    }
    if (!pds || !shader || !staged_available(m,&pds_gpu,&sz,1) || (pds->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) || (shader->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) || !(pds->mmu_flags&A04_GPU_MMU_READ_ONLY) ||
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
    staged_commit(m,&pds_gpu,&sz,1);
    return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_vertex_pds(struct a04_windows_render_memory *m,
    const struct a04_render_pds_input *input,uint64_t vertex,uint64_t descriptor)
{
    struct a04_render_owned_range ranges[A04_RENDER_MAX_MAPS];struct a04_render_pds_data data;
    uint64_t addr[4],len[4]={16,32,12,12},off[4],dma;size_t i,j,k;const uint8_t *src[4];
    if (!m || !input || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (vertex&15u) || (descriptor&15u) ||
        (vertex>>32)!=UINT64_C(0xda) || (descriptor>>32)!=UINT64_C(0xda) ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL || !render_ptes_valid(m)) return STATUS_INVALID_PARAMETER;
    dma=(uint64_t)m->logical.QuadPart;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];
        ranges[i].gpu_va=r->gpu_va;ranges[i].dma=dma+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        ranges[i].size=(uint64_t)r->pages*PAGE;ranges[i].access=A04_RENDER_RANGE_READ;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT))) ranges[i].access|=A04_RENDER_RANGE_WRITE;
        if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
            (r->gpu_va>>32)==UINT64_C(0xe0)) ranges[i].access|=A04_RENDER_RANGE_EXEC;
    }
    { unsigned matches=0;
      for(i=0;i<m->map_count;++i) if ((ranges[i].access&5u)==5u && input->shader_gpu>=ranges[i].gpu_va &&
          ranges[i].size>=312 && input->shader_gpu-ranges[i].gpu_va<=ranges[i].size-312) ++matches;
      if (matches!=1) return STATUS_INVALID_PARAMETER; }
    addr[0]=vertex;addr[1]=descriptor;addr[2]=input->spill_info_gpu;addr[3]=input->scratch_info_gpu;
    if (!staged_available(m,addr,len,4) || a04_render_make_pds_data(input,ranges,m->map_count,&data)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<4;++i) {
        unsigned matches=0;
        if (overlap(addr[i],len[i],input->shader_gpu,312) || overlap(addr[i],len[i],input->spill_buffer_gpu,8192) ||
            overlap(addr[i],len[i],input->scratch_buffer_gpu,49152)) return STATUS_INVALID_PARAMETER;
        for(j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t n=(uint64_t)r->pages*PAGE;
            if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
                addr[i]>=r->gpu_va && len[i]<=n && addr[i]-r->gpu_va<=n-len[i]) {
                off[i]=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr[i]-r->gpu_va;++matches;
            }
        }
        if (matches!=1 || off[i]>m->length || len[i]>m->length-off[i]) return STATUS_INVALID_PARAMETER;
        for(k=0;k<len[i];++k) if (((const uint8_t *)m->cpu)[off[i]+k]) return STATUS_INVALID_DEVICE_STATE;
    }
    src[0]=data.vertex;src[1]=data.descriptor;src[2]=data.spill_info;src[3]=data.scratch_info;
    for(i=0;i<4;++i) RtlCopyMemory((uint8_t *)m->cpu+off[i],src[i],i==0?12:i==1?24:12);
    staged_commit(m,addr,len,4);return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_scratchless_pds(struct a04_windows_render_memory *m,
    const struct a04_render_pds_input *input,uint64_t vertex,uint64_t descriptor)
{
    struct a04_render_owned_range ranges[A04_RENDER_MAX_MAPS];struct a04_render_pds_data data;
    uint64_t addr[3],len[3]={16,16,12},off[3],dma;size_t i,j,k;const uint8_t *src[3];
    if (!m || !input || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (vertex&15u) || (descriptor&15u) ||
        (vertex>>32)!=UINT64_C(0xda) || (descriptor>>32)!=UINT64_C(0xda) ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL || !render_ptes_valid(m)) return STATUS_INVALID_PARAMETER;
    dma=(uint64_t)m->logical.QuadPart;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];
        ranges[i].gpu_va=r->gpu_va;ranges[i].dma=dma+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        ranges[i].size=(uint64_t)r->pages*PAGE;ranges[i].access=A04_RENDER_RANGE_READ;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT))) ranges[i].access|=A04_RENDER_RANGE_WRITE;
        if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
            (r->gpu_va>>32)==UINT64_C(0xe0)) ranges[i].access|=A04_RENDER_RANGE_EXEC;
    }
    addr[0]=vertex;addr[1]=descriptor;addr[2]=input->spill_info_gpu;
    if (!staged_available(m,addr,len,3) || a04_render_make_scratchless_pds_data(input,ranges,m->map_count,&data)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<3;++i) {
        unsigned matches=0;
        if (overlap(addr[i],len[i],input->shader_gpu,176) || overlap(addr[i],len[i],input->spill_buffer_gpu,8192)) return STATUS_INVALID_PARAMETER;
        for(j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t n=(uint64_t)r->pages*PAGE;
            if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
                addr[i]>=r->gpu_va && len[i]<=n && addr[i]-r->gpu_va<=n-len[i]) {
                off[i]=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr[i]-r->gpu_va;++matches;
            }
        }
        if (matches!=1 || off[i]>m->length || len[i]>m->length-off[i]) return STATUS_INVALID_PARAMETER;
        for(k=0;k<len[i];++k) if (((const uint8_t *)m->cpu)[off[i]+k]) return STATUS_INVALID_DEVICE_STATE;
    }
    src[0]=data.vertex;src[1]=data.descriptor;src[2]=data.spill_info;
    for(i=0;i<3;++i) RtlCopyMemory((uint8_t *)m->cpu+off[i],src[i],12);
    staged_commit(m,addr,len,3);return STATUS_SUCCESS;
}
static NTSTATUS stage_vertex_code(struct a04_windows_render_memory *m,
    uint64_t vertex,uint64_t descriptor,BOOLEAN scratchless)
{
    uint64_t addr[2]={vertex,descriptor},len[2]={16,16},off[2];uint8_t v[16],d[16];size_t i,j,k;
    if (!m || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
        !render_ptes_valid(m) || !staged_available(m,addr,len,2)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<2;++i) {
        unsigned matches=0;
        if ((addr[i]>>32)!=UINT64_C(0xda) || (addr[i]&15u)) return STATUS_INVALID_PARAMETER;
        for(j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t n=(uint64_t)r->pages*PAGE;
            if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
                addr[i]>=r->gpu_va && n>=16 && addr[i]-r->gpu_va<=n-16) {
                off[i]=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr[i]-r->gpu_va;++matches;
            }
        }
        if (matches!=1 || off[i]>m->length || 16u>m->length-off[i]) return STATUS_INVALID_PARAMETER;
        for(k=0;k<16;++k) if (((const uint8_t *)m->cpu)[off[i]+k]) return STATUS_INVALID_DEVICE_STATE;
    }
    if (scratchless) a04_render_make_scratchless_pds_code(v,d);
    else a04_render_make_vertex_pds_code(v,d);
    RtlCopyMemory((uint8_t *)m->cpu+off[0],v,16);RtlCopyMemory((uint8_t *)m->cpu+off[1],d,16);
    staged_commit(m,addr,len,2);return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_vertex_pds_code(struct a04_windows_render_memory *m,uint64_t v,uint64_t d)
{ return stage_vertex_code(m,v,d,FALSE); }
NTSTATUS a04_windows_render_stage_scratchless_pds_code(struct a04_windows_render_memory *m,uint64_t v,uint64_t d)
{ return stage_vertex_code(m,v,d,TRUE); }
NTSTATUS a04_windows_render_stage_fragment_uniform(struct a04_windows_render_memory *m,
    const struct a04_render_fragment_uniform_input *input,uint64_t uniform,uint64_t code)
{
    struct a04_render_owned_range ranges[A04_RENDER_MAX_MAPS];struct a04_render_fragment_uniform_data data;
    uint64_t a[4],n[4]={32,12,4,16},off[4],dma;size_t i,j,k;const uint8_t *src[4];
    if (!m || !input || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (uniform&15u) || (code&15u) ||
        (uniform>>32)!=UINT64_C(0xda) || (code>>32)!=UINT64_C(0xda) ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL || !render_ptes_valid(m)) return STATUS_INVALID_PARAMETER;
    dma=(uint64_t)m->logical.QuadPart;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];
        ranges[i].gpu_va=r->gpu_va;ranges[i].dma=dma+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        ranges[i].size=(uint64_t)r->pages*PAGE;ranges[i].access=A04_RENDER_RANGE_READ;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT))) ranges[i].access|=A04_RENDER_RANGE_WRITE;
    }
    a[0]=uniform;a[1]=input->spill_info_gpu;a[2]=input->fs_meta_gpu;a[3]=code;
    if (!staged_available(m,a,n,4) || a04_render_make_fragment_uniform(input,ranges,m->map_count,&data)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<4;++i) {
        unsigned matches=0;
        if (overlap(a[i],n[i],input->spill_buffer_gpu,8192)) return STATUS_INVALID_PARAMETER;
        for(j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t bytes=(uint64_t)r->pages*PAGE;
            if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
                a[i]>=r->gpu_va && n[i]<=bytes && a[i]-r->gpu_va<=bytes-n[i]) {
                off[i]=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+a[i]-r->gpu_va;++matches;
            }
        }
        if (matches!=1 || off[i]>m->length || n[i]>m->length-off[i]) return STATUS_INVALID_PARAMETER;
        for(k=0;k<n[i];++k) if (((const uint8_t *)m->cpu)[off[i]+k]) return STATUS_INVALID_DEVICE_STATE;
    }
    src[0]=data.descriptor;src[1]=data.spill_info;src[2]=data.fs_meta;src[3]=data.code;
    RtlCopyMemory((uint8_t *)m->cpu+off[0],src[0],24);RtlCopyMemory((uint8_t *)m->cpu+off[1],src[1],12);
    RtlCopyMemory((uint8_t *)m->cpu+off[2],src[2],4);RtlCopyMemory((uint8_t *)m->cpu+off[3],src[3],16);
    staged_commit(m,a,n,4);return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_fixed_ppp(struct a04_windows_render_memory *m,
    const struct a04_render_fixed_ppp_input *input,uint64_t ppp,uint64_t finish)
{
    struct a04_render_owned_range ranges[A04_RENDER_MAX_MAPS];struct a04_render_fixed_ppp data;
    uint64_t addr[2]={ppp,finish},len[2]={100,12},off[2],deps[3],sizes[3]={48,16,32},dma;size_t i,j,k;
    if (!m || !input || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (ppp&3u) || (finish&3u) ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL || !render_ptes_valid(m) || !staged_available(m,addr,len,2)) return STATUS_INVALID_PARAMETER;
    deps[0]=input->fragment_pds;deps[1]=input->uniform_code;deps[2]=input->uniform_data;dma=(uint64_t)m->logical.QuadPart;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];
        ranges[i].gpu_va=r->gpu_va;ranges[i].dma=dma+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        ranges[i].size=(uint64_t)r->pages*PAGE;ranges[i].access=A04_RENDER_RANGE_READ;
        if (!(r->mmu_flags&A04_GPU_MMU_READ_ONLY) || (r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT)) ranges[i].access|=A04_RENDER_RANGE_WRITE;
    }
    if (a04_render_make_fixed_ppp(input,ranges,m->map_count,&data)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<2;++i) {
        unsigned matches=0;
        for(j=0;j<3;++j) if(overlap(addr[i],len[i],deps[j],sizes[j])) return STATUS_INVALID_PARAMETER;
        for(j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t n=(uint64_t)r->pages*PAGE;
            if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
                addr[i]>=r->gpu_va && len[i]<=n && addr[i]-r->gpu_va<=n-len[i]) {
                off[i]=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr[i]-r->gpu_va;++matches;
            }
        }
        if (matches!=1 || off[i]>m->length || len[i]>m->length-off[i]) return STATUS_INVALID_PARAMETER;
        for(k=0;k<len[i];++k) if (((const uint8_t *)m->cpu)[off[i]+k]) return STATUS_INVALID_DEVICE_STATE;
    }
    RtlCopyMemory((uint8_t *)m->cpu+off[0],data.state,sizeof(data.state));
    RtlCopyMemory((uint8_t *)m->cpu+off[1],data.finish,sizeof(data.finish));
    staged_commit(m,addr,len,2);return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_pixel_event(struct a04_windows_render_memory *m,uint64_t pds,uint64_t shader)
{
    struct a04_render_owned_range ranges[A04_RENDER_MAX_MAPS];uint64_t len=16,off=0;uint8_t data[16];size_t i;unsigned matches=0;
    if (!m || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (pds&15u) || (pds>>32)!=UINT64_C(0xda) ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL || !render_ptes_valid(m) || !staged_available(m,&pds,&len,1) ||
        overlap(pds,len,shader,128)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];uint64_t bytes=(uint64_t)r->pages*PAGE;
        ranges[i].gpu_va=r->gpu_va;ranges[i].dma=(uint64_t)m->logical.QuadPart+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        ranges[i].size=bytes;ranges[i].access=A04_RENDER_RANGE_READ;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT))) ranges[i].access|=A04_RENDER_RANGE_WRITE;
        if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) && (r->gpu_va>>32)==UINT64_C(0xe0))
            ranges[i].access|=A04_RENDER_RANGE_EXEC;
        if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
            pds>=r->gpu_va && bytes>=len && pds-r->gpu_va<=bytes-len) {
            off=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+pds-r->gpu_va;++matches;
        }
    }
    if (matches!=1 || off>m->length || len>m->length-off || a04_render_make_pixel_event(shader,ranges,m->map_count,data))
        return STATUS_INVALID_PARAMETER;
    for(i=0;i<len;++i) if (((const uint8_t *)m->cpu)[off+i]) return STATUS_INVALID_DEVICE_STATE;
    RtlCopyMemory((uint8_t *)m->cpu+off,data,sizeof(data));staged_commit(m,&pds,&len,1);return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_fixed_eot(struct a04_windows_render_memory *m,uint64_t shader,uint64_t target)
{
    struct a04_render_owned_range ranges[A04_RENDER_MAX_MAPS];
    uint64_t addr[2]={shader,target},len[2]={128,16384},off=0;uint8_t data[128];size_t i;unsigned matches=0;
    if (!m || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (shader&15u) || (shader>>32)!=UINT64_C(0xe0) ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL || !render_ptes_valid(m) || !staged_available(m,addr,len,2))
        return STATUS_INVALID_PARAMETER;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];uint64_t bytes=(uint64_t)r->pages*PAGE;
        ranges[i].gpu_va=r->gpu_va;ranges[i].dma=(uint64_t)m->logical.QuadPart+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        ranges[i].size=bytes;ranges[i].access=A04_RENDER_RANGE_READ;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT))) ranges[i].access|=A04_RENDER_RANGE_WRITE;
        if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
            shader>=r->gpu_va && bytes>=len[0] && shader-r->gpu_va<=bytes-len[0]) {
            off=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+shader-r->gpu_va;++matches;
        }
    }
    if (matches!=1 || off>m->length || len[0]>m->length-off ||
        a04_render_make_fixed_eot(target,ranges,m->map_count,data)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<len[0];++i) if (((const uint8_t *)m->cpu)[off+i]) return STATUS_INVALID_DEVICE_STATE;
    RtlCopyMemory((uint8_t *)m->cpu+off,data,sizeof(data));staged_commit(m,addr,len,2);return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_fixed_background(struct a04_windows_render_memory *m,uint64_t constants,uint64_t pds,uint64_t code)
{
    struct a04_render_owned_range ranges[A04_RENDER_MAX_MAPS];
    uint64_t addr[3]={constants,pds,code},len[3]={16,16,16},off[3];uint8_t c[16],d[16];size_t i,j,k;
    static const uint8_t instructions[16]={0,0,2,0xf4,0,0,0,0xd3};
    if (!m || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (pds&15u) || (pds>>32)!=UINT64_C(0xda) ||
        (code&15u) || (code>>32)!=UINT64_C(0xda) || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
        !render_ptes_valid(m) || !staged_available(m,addr,len,3))
        return STATUS_INVALID_PARAMETER;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];
        ranges[i].gpu_va=r->gpu_va;ranges[i].dma=(uint64_t)m->logical.QuadPart+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        ranges[i].size=(uint64_t)r->pages*PAGE;ranges[i].access=A04_RENDER_RANGE_READ;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT))) ranges[i].access|=A04_RENDER_RANGE_WRITE;
    }
    if (a04_render_make_fixed_background(constants,ranges,m->map_count,c,d)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<3;++i) {
        unsigned matches=0;
        for(j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t bytes=(uint64_t)r->pages*PAGE;
            if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
                addr[i]>=r->gpu_va && bytes>=len[i] && addr[i]-r->gpu_va<=bytes-len[i]) {
                off[i]=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr[i]-r->gpu_va;++matches;
            }
        }
        if (matches!=1 || off[i]>m->length || len[i]>m->length-off[i]) return STATUS_INVALID_PARAMETER;
        for(k=0;k<len[i];++k) if (((const uint8_t *)m->cpu)[off[i]+k]) return STATUS_INVALID_DEVICE_STATE;
    }
    RtlCopyMemory((uint8_t *)m->cpu+off[0],c,16);RtlCopyMemory((uint8_t *)m->cpu+off[1],d,16);
    RtlCopyMemory((uint8_t *)m->cpu+off[2],instructions,16);
    staged_commit(m,addr,len,3);return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_background_shader(struct a04_windows_render_memory *m,uint64_t shader,uint64_t pds)
{
    struct a04_render_owned_range ranges[A04_RENDER_MAX_MAPS];
    uint64_t addr[2]={shader,pds},len[2]={128,48},off[2];uint8_t usc[128],data[48];size_t i,j,k;
    if (!m || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (shader&15u) || (shader>>32)!=UINT64_C(0xe0) ||
        (pds&15u) || (pds>>32)!=UINT64_C(0xda) || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
        !render_ptes_valid(m) || !staged_available(m,addr,len,2)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];
        ranges[i].gpu_va=r->gpu_va;ranges[i].dma=(uint64_t)m->logical.QuadPart+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        ranges[i].size=(uint64_t)r->pages*PAGE;ranges[i].access=A04_RENDER_RANGE_READ;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT))) ranges[i].access|=A04_RENDER_RANGE_WRITE;
        if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) && (r->gpu_va>>32)==UINT64_C(0xe0)) ranges[i].access|=A04_RENDER_RANGE_EXEC;
    }
    if (a04_render_make_fragment_pds(shader,0,ranges,m->map_count,data)) return STATUS_INVALID_PARAMETER;
    a04_render_make_fixed_background_usc(usc);
    for(i=0;i<2;++i) {
        unsigned matches=0;
        for(j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t bytes=(uint64_t)r->pages*PAGE;
            if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
                addr[i]>=r->gpu_va && bytes>=len[i] && addr[i]-r->gpu_va<=bytes-len[i]) {
                off[i]=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr[i]-r->gpu_va;++matches;
            }
        }
        if (matches!=1 || off[i]>m->length || len[i]>m->length-off[i]) return STATUS_INVALID_PARAMETER;
        for(k=0;k<len[i];++k) if (((const uint8_t *)m->cpu)[off[i]+k]) return STATUS_INVALID_DEVICE_STATE;
    }
    RtlCopyMemory((uint8_t *)m->cpu+off[0],usc,sizeof(usc));RtlCopyMemory((uint8_t *)m->cpu+off[1],data,sizeof(data));
    staged_commit(m,addr,len,2);return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_fixed_scissor(struct a04_windows_render_memory *m,uint64_t gpu)
{
    uint64_t len=8,off=0;size_t i;unsigned matches=0;
    static const uint8_t data[8]={64,0,0,0,64,0,0,0};
    if (!m || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (gpu&3u) || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
        !render_ptes_valid(m) || !staged_available(m,&gpu,&len,1)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];uint64_t bytes=(uint64_t)r->pages*PAGE;
        if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
            gpu>=r->gpu_va && bytes>=len && gpu-r->gpu_va<=bytes-len) {
            off=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+gpu-r->gpu_va;++matches;
        }
    }
    if (matches!=1 || off>m->length || len>m->length-off) return STATUS_INVALID_PARAMETER;
    for(i=0;i<len;++i) if (((const uint8_t *)m->cpu)[off+i]) return STATUS_INVALID_DEVICE_STATE;
    RtlCopyMemory((uint8_t *)m->cpu+off,data,sizeof(data));staged_commit(m,&gpu,&len,1);return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_fixed_vdm(struct a04_windows_render_memory *m,
    const struct a04_render_fixed_vdm_input *input,uint64_t vdm)
{
    struct a04_render_owned_range ranges[A04_RENDER_MAX_MAPS];uint64_t a[6],n[6]={16,16,16,16,100,12},off=0,len=60;
    uint8_t data[60];size_t i;unsigned matches=0;
    if (!m || !input || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (vdm&3u) || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
        !render_ptes_valid(m) || !staged_available(m,&vdm,&len,1)) return STATUS_INVALID_PARAMETER;
    a[0]=input->descriptor_data;a[1]=input->descriptor_code;a[2]=input->vertex_data;
    a[3]=input->vertex_code;a[4]=input->ppp_state;a[5]=input->ppp_finish;
    for(i=0;i<6;++i) if(overlap(vdm,len,a[i],n[i])) return STATUS_INVALID_PARAMETER;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];uint64_t bytes=(uint64_t)r->pages*PAGE;
        ranges[i].gpu_va=r->gpu_va;ranges[i].dma=(uint64_t)m->logical.QuadPart+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        ranges[i].size=bytes;ranges[i].access=A04_RENDER_RANGE_READ;
        if (!(r->mmu_flags&A04_GPU_MMU_READ_ONLY) || (r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT)) ranges[i].access|=A04_RENDER_RANGE_WRITE;
        if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
            vdm>=r->gpu_va && bytes>=len && vdm-r->gpu_va<=bytes-len) {
            off=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+vdm-r->gpu_va;++matches;
        }
    }
    if (matches!=1 || off>m->length || len>m->length-off || a04_render_make_fixed_vdm(input,ranges,m->map_count,data))
        return STATUS_INVALID_PARAMETER;
    for(i=0;i<len;++i) if (((const uint8_t *)m->cpu)[off+i]) return STATUS_INVALID_DEVICE_STATE;
    RtlCopyMemory((uint8_t *)m->cpu+off,data,sizeof(data));staged_commit(m,&vdm,&len,1);return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_context_pt(struct a04_windows_render_memory *m,
    uint64_t state,uint64_t store_data,uint64_t store_code,uint64_t resume_data,uint64_t resume_code)
{
    uint64_t addr[4]={store_data,store_code,resume_data,resume_code},len[4]={32,32,32,32},off[4],dma,stateoff=0,state_len=128;size_t i,j;
    struct a04_render_owned_range range={0};struct a04_render_context_pt_data data;
    struct a04_render_context_pds_code code;const uint8_t *src[4];
    if (!m || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || m->pt_slot_count>=2u || (state>>32)!=UINT64_C(0xda) || (state&15u) ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL || !render_ptes_valid(m)) return STATUS_INVALID_PARAMETER;
    dma=(uint64_t)m->logical.QuadPart;
    for (i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];uint64_t n=(uint64_t)r->pages*PAGE;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT)) && state>=r->gpu_va && n>=128 && state-r->gpu_va<=n-128) {
            if (range.size) return STATUS_INVALID_PARAMETER;
            range.gpu_va=r->gpu_va;range.size=n;range.dma=dma+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;range.access=3;
            stateoff=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+state-r->gpu_va;
        }
    }
    if (!range.size || !writable_available(m,&state,&state_len,1) || !staged_available(m,addr,len,4) ||
        stateoff>m->length || state_len>m->length-stateoff ||
        a04_render_make_context_pt_data(state,&range,1,&data) || a04_render_make_context_pds_code(&code))
        return STATUS_INVALID_PARAMETER;
    for(i=0;i<state_len;++i) if (((const uint8_t *)m->cpu)[stateoff+i]) return STATUS_INVALID_DEVICE_STATE;
    for (i=0;i<4;++i) {
        unsigned matches=0;
        if ((addr[i]>>32)!=UINT64_C(0xda) || (addr[i]&15u) || overlap(addr[i],32,state,128)) return STATUS_INVALID_PARAMETER;
        for(j=0;j<i;++j) if(overlap(addr[i],32,addr[j],32)) return STATUS_INVALID_PARAMETER;
        for (j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t n=(uint64_t)r->pages*PAGE;
            if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
                addr[i]>=r->gpu_va && n>=32 && addr[i]-r->gpu_va<=n-32) {
                off[i]=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr[i]-r->gpu_va;++matches;
            }
        }
        if (matches!=1 || off[i]>m->length || 32u>m->length-off[i]) return STATUS_INVALID_PARAMETER;
        for(j=0;j<32u;++j) if (((const uint8_t *)m->cpu)[off[i]+j]) return STATUS_INVALID_DEVICE_STATE;
    }
    src[0]=data.store;src[1]=code.pt_store;src[2]=data.resume;src[3]=code.pt_resume;
    for (i=0;i<4;++i) for(j=0;j<32;++j) ((uint8_t *)m->cpu)[off[i]+j]=src[i][j];
    staged_commit(m,addr,len,4);writable_commit(m,&state,&state_len,1);
    i=m->pt_slot_count++;m->pt_slots[i].state=state;m->pt_slots[i].store_data=store_data;
    m->pt_slots[i].store_code=store_code;m->pt_slots[i].resume_data=resume_data;m->pt_slots[i].resume_code=resume_code;
    return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_bounded_context_usc(struct a04_windows_render_memory *m,uint64_t store,uint64_t load)
{
    uint64_t addr[2]={store,load},len[2]={768,640},off[2];size_t i,j,k;
    const uint8_t *code[2]={a04_context_store_bounded,a04_context_load_bounded};
    const size_t code_size[2]={sizeof(a04_context_store_bounded),sizeof(a04_context_load_bounded)};
    if (!m || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || KeGetCurrentIrql()!=PASSIVE_LEVEL ||
        !render_ptes_valid(m) || !staged_available(m,addr,len,2)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<2;++i) {
        unsigned matches=0;
        if ((addr[i]>>32)!=UINT64_C(0xe0) || (addr[i]&63u)) return STATUS_INVALID_PARAMETER;
        for(j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t bytes=(uint64_t)r->pages*PAGE;
            if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
                addr[i]>=r->gpu_va && len[i]<=bytes && addr[i]-r->gpu_va<=bytes-len[i]) {
                off[i]=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr[i]-r->gpu_va;++matches;
            }
        }
        if (matches!=1 || off[i]>m->length || len[i]>m->length-off[i]) return STATUS_INVALID_PARAMETER;
        for(k=0;k<len[i];++k) if (((const uint8_t *)m->cpu)[off[i]+k]) return STATUS_INVALID_DEVICE_STATE;
    }
    for(i=0;i<2;++i) RtlCopyMemory((uint8_t *)m->cpu+off[i],code[i],code_size[i]);
    staged_commit(m,addr,len,2);return STATUS_SUCCESS;
}
static NTSTATUS render_stage_context_sr(struct a04_windows_render_memory *m,
    uint64_t state,uint64_t store_shader,uint64_t load_shader,uint64_t store_ctl,uint64_t load_ctl,
    uint64_t store_data,uint64_t store_code,uint64_t load_data,uint64_t load_code,BOOLEAN bounded)
{
    uint64_t addr[4]={store_data,store_code,load_data,load_code},length[4]={64,16,64,16},off[4];
    uint64_t sh[2]={store_shader,load_shader},shsize[2]={544,488},dma,stateoff=0,state_len=16432;size_t i,j,k;unsigned state_matches=0;
    struct a04_render_owned_range ranges[A04_RENDER_MAX_MAPS];struct a04_render_context_pds_code code;
    uint8_t sd[64],ld[64];const uint8_t *src[4];
    if (bounded) { shsize[0]=768;shsize[1]=640; }
    if (!m || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || (state>>32)!=UINT64_C(0xda) ||
        (state&15u) || KeGetCurrentIrql()!=PASSIVE_LEVEL || !render_ptes_valid(m)) return STATUS_INVALID_PARAMETER;
    dma=(uint64_t)m->logical.QuadPart;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];
        ranges[i].gpu_va=r->gpu_va;ranges[i].dma=dma+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        ranges[i].size=(uint64_t)r->pages*PAGE;
        ranges[i].access=A04_RENDER_RANGE_READ;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT))) ranges[i].access|=A04_RENDER_RANGE_WRITE;
        if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) && (r->gpu_va>>32)==UINT64_C(0xe0)) ranges[i].access|=A04_RENDER_RANGE_EXEC;
        if ((ranges[i].access&3u)==3u && state>=r->gpu_va && ranges[i].size>=state_len && state-r->gpu_va<=ranges[i].size-state_len) {
            stateoff=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+state-r->gpu_va;++state_matches;
        }
    }
    for(i=0;i<2;++i) {
        unsigned matches=0;
        uint64_t shoff=0;unsigned reserved=0;
        for(j=0;j<m->map_count;++j) if ((ranges[j].access&5u)==5u && sh[i]>=ranges[j].gpu_va &&
            ranges[j].size>=shsize[i] && sh[i]-ranges[j].gpu_va<=ranges[j].size-shsize[i]) {
            shoff=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)m->maps[j].payload_page*PAGE+sh[i]-ranges[j].gpu_va;++matches;
        }
        if (matches!=1 || (sh[i]&3u) || (sh[i]>>32)!=UINT64_C(0xe0)) return STATUS_INVALID_PARAMETER;
        if (bounded) {
            const uint8_t *expected=i?a04_context_load_bounded:a04_context_store_bounded;
            size_t used=i?sizeof(a04_context_load_bounded):sizeof(a04_context_store_bounded);
            for(j=0;j<m->staged_count && j<A04_RENDER_MAX_STAGED;++j)
                if(m->staged[j].gpu_va==sh[i] && m->staged[j].size==shsize[i]) ++reserved;
            if (reserved!=1 || shoff>m->length || shsize[i]>m->length-shoff) return STATUS_INVALID_DEVICE_STATE;
            for(j=0;j<shsize[i];++j)
                if (((const uint8_t *)m->cpu)[shoff+j]!=(j<used?expected[j]:0)) return STATUS_INVALID_DEVICE_STATE;
        }
    }
    if (state_matches!=1 || stateoff>m->length || state_len>m->length-stateoff ||
        !writable_available(m,&state,&state_len,1) || !staged_available(m,addr,length,4) || overlap(sh[0],shsize[0],sh[1],shsize[1]) ||
        a04_render_make_context_sr_data(state,sh[0],store_ctl,ranges,m->map_count,sd) ||
        a04_render_make_context_sr_data(state,sh[1],load_ctl,ranges,m->map_count,ld) ||
        a04_render_make_context_pds_code(&code)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<state_len;++i) if (((const uint8_t *)m->cpu)[stateoff+i]) return STATUS_INVALID_DEVICE_STATE;
    for(i=0;i<4;++i) {
        unsigned matches=0;
        if ((addr[i]>>32)!=UINT64_C(0xda) || (addr[i]&15u) || overlap(addr[i],length[i],state,16432)) return STATUS_INVALID_PARAMETER;
        for(j=0;j<i;++j) if(overlap(addr[i],length[i],addr[j],length[j])) return STATUS_INVALID_PARAMETER;
        for(j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t n=(uint64_t)r->pages*PAGE;
            if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
                addr[i]>=r->gpu_va && length[i]<=n && addr[i]-r->gpu_va<=n-length[i]) {
                off[i]=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr[i]-r->gpu_va;++matches;
            }
        }
        if (matches!=1 || off[i]>m->length || length[i]>m->length-off[i]) return STATUS_INVALID_PARAMETER;
        for(k=0;k<length[i];++k) if (((const uint8_t *)m->cpu)[off[i]+k]) return STATUS_INVALID_DEVICE_STATE;
    }
    src[0]=sd;src[1]=code.sr;src[2]=ld;src[3]=code.sr;
    for(i=0;i<4;++i) for(j=0;j<length[i];++j) ((uint8_t *)m->cpu)[off[i]+j]=src[i][j];
    staged_commit(m,addr,length,4);writable_commit(m,&state,&state_len,1);
    return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_stage_context_sr(struct a04_windows_render_memory *m,
    uint64_t state,uint64_t store_shader,uint64_t load_shader,uint64_t store_ctl,uint64_t load_ctl,
    uint64_t store_data,uint64_t store_code,uint64_t load_data,uint64_t load_code)
{
    NTSTATUS status;size_t i;
    if (!m || m->sr_slot_count>=2u) return STATUS_INVALID_PARAMETER;
    status=render_stage_context_sr(m,state,store_shader,load_shader,store_ctl,load_ctl,
        store_data,store_code,load_data,load_code,FALSE);
    if (NT_SUCCESS(status)) {
        i=m->sr_slot_count++;m->sr_slots[i].state=state;m->sr_slots[i].store_shader=store_shader;m->sr_slots[i].load_shader=load_shader;
        m->sr_slots[i].store_data=store_data;m->sr_slots[i].store_code=store_code;m->sr_slots[i].load_data=load_data;m->sr_slots[i].load_code=load_code;
        m->sr_slots[i].bounded=FALSE;
    }
    return status;
}
NTSTATUS a04_windows_render_stage_bounded_context_sr(struct a04_windows_render_memory *m,
    uint64_t state,uint64_t store_shader,uint64_t load_shader,
    uint64_t store_data,uint64_t store_code,uint64_t load_data,uint64_t load_code)
{
    NTSTATUS status;size_t i;
    /* PCO metadata: store uses 11 and load 10 temps, both rounded to 3
     * four-register units at DOUTU bit 35. */
    if (!m || m->bounded_state_count>=2u || m->sr_slot_count>=2u) return STATUS_INVALID_PARAMETER;
    status=render_stage_context_sr(m,state,store_shader,load_shader,UINT64_C(3)<<35,UINT64_C(3)<<35,
        store_data,store_code,load_data,load_code,TRUE);
    if (NT_SUCCESS(status)) {
        m->bounded_states[m->bounded_state_count++].gpu_va=state;
        i=m->sr_slot_count++;m->sr_slots[i].state=state;m->sr_slots[i].store_shader=store_shader;m->sr_slots[i].load_shader=load_shader;
        m->sr_slots[i].store_data=store_data;m->sr_slots[i].store_code=store_code;m->sr_slots[i].load_data=load_data;m->sr_slots[i].load_code=load_code;
        m->sr_slots[i].bounded=TRUE;
    }
    return status;
}
NTSTATUS a04_windows_render_check_bounded_states(struct a04_windows_render_memory *m)
{
    size_t i,j;int bad=0;
    if (!m || !m->completion_observed || !m->cpu || m->bounded_state_count>2u ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    m->bounded_states_checked=FALSE;
    for(i=0;i<m->bounded_state_count;++i) {
        uint64_t addr=m->bounded_states[i].gpu_va,off=0;unsigned matches=0;
        struct a04_render_shared_size_result result;
        m->bounded_states[i].valid=FALSE;m->bounded_states[i].blocks=0;
        for(j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t size=(uint64_t)r->pages*PAGE;
            if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT)) &&
                addr>=r->gpu_va && size>=16432u && addr-r->gpu_va<=size-16432u) {
                off=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr-r->gpu_va;++matches;
            }
        }
        if (matches!=1 || off>m->length || 16432u>m->length-off ||
            a04_render_check_bounded_shared_size((const uint8_t *)m->cpu+off,16432,&result)) { bad=1;continue; }
        m->bounded_states[i].valid=TRUE;m->bounded_states[i].blocks=result.blocks;
    }
    m->bounded_states_checked=TRUE;return bad?STATUS_DATA_ERROR:STATUS_SUCCESS;
}
static int staged_bytes_equal(const struct a04_windows_render_memory *m,uint64_t addr,size_t reserved,
    const uint8_t *expected,size_t used)
{
    size_t i,j;uint64_t off=0;unsigned maps=0,records=0;
    if (!expected || used>reserved || m->staged_count>A04_RENDER_MAX_STAGED) return 0;
    for(i=0;i<m->staged_count;++i) if(m->staged[i].gpu_va==addr && m->staged[i].size==reserved) ++records;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];uint64_t size=(uint64_t)r->pages*PAGE;
        if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) &&
            addr>=r->gpu_va && reserved<=size && addr-r->gpu_va<=size-reserved) {
            off=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr-r->gpu_va;++maps;
        }
    }
    if (records!=1 || maps!=1 || off>m->length || reserved>m->length-off) return 0;
    for(j=0;j<reserved;++j) if (((const uint8_t *)m->cpu)[off+j]!=(j<used?expected[j]:0)) return 0;
    return 1;
}
static int bounded_programs_valid(const struct a04_windows_render_memory *m,
    const struct a04_render_owned_range *ranges)
{
    size_t i;struct a04_render_context_pds_code code;
    if (m->pt_slot_count!=2 || m->sr_slot_count!=2 || m->bounded_state_count!=2 ||
        a04_render_make_context_pds_code(&code)) return 0;
    for(i=0;i<2;++i) {
        struct a04_render_context_pt_data pt;uint8_t st[64],ld[64];
        if (m->bounded_states[i].gpu_va!=m->sr_slots[i].state ||
            a04_render_make_context_pt_data(m->pt_slots[i].state,ranges,m->map_count,&pt) ||
            a04_render_make_context_sr_data(m->sr_slots[i].state,m->sr_slots[i].store_shader,UINT64_C(3)<<35,ranges,m->map_count,st) ||
            a04_render_make_context_sr_data(m->sr_slots[i].state,m->sr_slots[i].load_shader,UINT64_C(3)<<35,ranges,m->map_count,ld) ||
            !staged_bytes_equal(m,m->pt_slots[i].store_data,32,pt.store,32) ||
            !staged_bytes_equal(m,m->pt_slots[i].store_code,32,code.pt_store,32) ||
            !staged_bytes_equal(m,m->pt_slots[i].resume_data,32,pt.resume,32) ||
            !staged_bytes_equal(m,m->pt_slots[i].resume_code,32,code.pt_resume,32) ||
            !staged_bytes_equal(m,m->sr_slots[i].store_data,64,st,64) ||
            !staged_bytes_equal(m,m->sr_slots[i].store_code,16,code.sr,16) ||
            !staged_bytes_equal(m,m->sr_slots[i].load_data,64,ld,64) ||
            !staged_bytes_equal(m,m->sr_slots[i].load_code,16,code.sr,16) ||
            !staged_bytes_equal(m,m->sr_slots[i].store_shader,768,a04_context_store_bounded,sizeof(a04_context_store_bounded)) ||
            !staged_bytes_equal(m,m->sr_slots[i].load_shader,640,a04_context_load_bounded,sizeof(a04_context_load_bounded)))
            return 0;
    }
    return 1;
}
static int bounded_state_zero(const struct a04_windows_render_memory *m,uint64_t addr,size_t len,int uncached)
{
    size_t i,j;unsigned matches=0,records=0;uint64_t off=0;
    for(i=0;i<m->writable_count && i<A04_RENDER_MAX_WRITABLE;++i)
        if(m->writable[i].gpu_va==addr && m->writable[i].size==len) ++records;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];uint64_t size=(uint64_t)r->pages*PAGE;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT)) && (!uncached || (r->mmu_flags&A04_GPU_MMU_SLC_BYPASS)) &&
            addr>=r->gpu_va && len<=size && addr-r->gpu_va<=size-len) {
            off=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr-r->gpu_va;++matches;
        }
    }
    if (matches!=1 || records!=1 || off>m->length || len>m->length-off) return 0;
    for(j=0;j<len;++j) if (((const uint8_t *)m->cpu)[off+j]) return 0;
    return 1;
}
static int bounded_live_valid(const struct a04_windows_render_memory *m)
{
    struct a04_render_owned_range ranges[A04_RENDER_MAX_MAPS];struct a04_render_context_static_input p={0};
    struct rogue_fwif_geom_registers_caswitch result;size_t i;
    if (!m->bounded_static_prepared || m->pt_slot_count!=2 || m->sr_slot_count!=2 || m->bounded_state_count!=2 ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || !reservations_valid(m)) return 0;
    p.vdm_state=m->bounded_static_state.geom_reg_vdm_context_state_base_addr;
    p.geom_state=m->bounded_static_state.geom_reg_ta_context_state_base_addr;
    for(i=0;i<2;++i) {
        struct a04_render_context_static_slot *s=&p.slot[i];
        if (!m->sr_slots[i].bounded || !bounded_state_zero(m,m->pt_slots[i].state,128,0) ||
            !bounded_state_zero(m,m->sr_slots[i].state,16432,0)) return 0;
        s->pt_store_data=m->pt_slots[i].store_data;s->pt_store_code=m->pt_slots[i].store_code;
        s->pt_resume_data=m->pt_slots[i].resume_data;s->pt_resume_code=m->pt_slots[i].resume_code;
        s->sr_store_data=m->sr_slots[i].store_data;s->sr_store_code=m->sr_slots[i].store_code;
        s->sr_resume_data=m->sr_slots[i].load_data;s->sr_resume_code=m->sr_slots[i].load_code;
    }
    if (!bounded_state_zero(m,p.vdm_state,92,1) || !bounded_state_zero(m,p.geom_state,484,1)) return 0;
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];
        ranges[i].gpu_va=r->gpu_va;ranges[i].dma=(uint64_t)m->logical.QuadPart+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        ranges[i].size=(uint64_t)r->pages*PAGE;ranges[i].access=A04_RENDER_RANGE_READ;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT))) ranges[i].access|=A04_RENDER_RANGE_WRITE;
        if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) && (r->gpu_va>>32)==UINT64_C(0xe0)) ranges[i].access|=A04_RENDER_RANGE_EXEC;
    }
    return !a04_render_make_context_static(&p,ranges,m->map_count,&result) &&
        bytes_equal(&result,&m->bounded_static_state,sizeof(result)) && bounded_programs_valid(m,ranges);
}
NTSTATUS a04_windows_render_make_bounded_static(struct a04_windows_render_memory *m,
    const struct a04_render_context_static_input *input,struct rogue_fwif_geom_registers_caswitch *out)
{
    struct a04_render_owned_range ranges[A04_RENDER_MAX_MAPS];struct rogue_fwif_geom_registers_caswitch result;
    struct a04_render_context_static_input p;
    uint64_t addr[2],len[2]={92,484},off[2];size_t i,j,k;
    if (!m || !input || !out || !m->ready || !m->cpu || !m->adapter || !m->firmware || m->published || m->poisoned ||
        m->render_prepared || m->bounded_static_prepared || m->pt_slot_count!=2 || m->sr_slot_count!=2 || m->bounded_state_count!=2 ||
        !m->map_count || m->map_count>A04_RENDER_MAX_MAPS || KeGetCurrentIrql()!=PASSIVE_LEVEL || !render_ptes_valid(m))
        return STATUS_INVALID_PARAMETER;
    p=*input;addr[0]=p.vdm_state;addr[1]=p.geom_state;
    if (!writable_available(m,addr,len,2)) return STATUS_INVALID_PARAMETER;
    for(i=0;i<2;++i) {
        const struct a04_render_context_static_slot *s=&p.slot[i];
        if (!m->sr_slots[i].bounded || s->pt_store_data!=m->pt_slots[i].store_data ||
            s->pt_store_code!=m->pt_slots[i].store_code || s->pt_resume_data!=m->pt_slots[i].resume_data ||
            s->pt_resume_code!=m->pt_slots[i].resume_code || s->sr_store_data!=m->sr_slots[i].store_data ||
            s->sr_store_code!=m->sr_slots[i].store_code || s->sr_resume_data!=m->sr_slots[i].load_data ||
            s->sr_resume_code!=m->sr_slots[i].load_code) return STATUS_INVALID_PARAMETER;
    }
    for(i=0;i<m->map_count;++i) {
        const struct a04_windows_render_map *r=&m->maps[i];
        ranges[i].gpu_va=r->gpu_va;ranges[i].dma=(uint64_t)m->logical.QuadPart+A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE;
        ranges[i].size=(uint64_t)r->pages*PAGE;ranges[i].access=A04_RENDER_RANGE_READ;
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT))) ranges[i].access|=A04_RENDER_RANGE_WRITE;
        if ((r->mmu_flags&A04_GPU_MMU_READ_ONLY) && !(r->mmu_flags&A04_GPU_MMU_PM_FW_PROTECT) && (r->gpu_va>>32)==UINT64_C(0xe0)) ranges[i].access|=A04_RENDER_RANGE_EXEC;
    }
    if (!reservations_valid(m) || a04_render_make_context_static(&p,ranges,m->map_count,&result)) return STATUS_INVALID_PARAMETER;
    if (!bounded_programs_valid(m,ranges)) return STATUS_INVALID_DEVICE_STATE;
    for(i=0;i<2;++i) {
        unsigned matches=0;
        for(j=0;j<m->map_count;++j) {
            const struct a04_windows_render_map *r=&m->maps[j];uint64_t size=(uint64_t)r->pages*PAGE;
            if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT)) && (r->mmu_flags&A04_GPU_MMU_SLC_BYPASS) &&
                addr[i]>=r->gpu_va && len[i]<=size && addr[i]-r->gpu_va<=size-len[i]) {
                off[i]=A04_RENDER_PAYLOAD_OFFSET+(uint64_t)r->payload_page*PAGE+addr[i]-r->gpu_va;++matches;
            }
        }
        if (matches!=1 || off[i]>m->length || len[i]>m->length-off[i]) return STATUS_INVALID_PARAMETER;
        for(k=0;k<len[i];++k) if (((const uint8_t *)m->cpu)[off[i]+k]) return STATUS_INVALID_DEVICE_STATE;
    }
    writable_commit(m,addr,len,2);m->bounded_static_state=result;m->bounded_static_prepared=TRUE;*out=result;return STATUS_SUCCESS;
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
    if (vm->pc_dev_paddr!=(uint64_t)m->logical.QuadPart || vm->page_cat_base_reg_set!=ROGUE_FW_BIF_INVALID_PCSET ||
        !render_ptes_valid(m) || !reservations_valid(m))
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
    if (m->bounded_static_prepared &&
        (!bytes_equal(&ctx->static_render_context_state.ctxswitch_regs[0],&m->bounded_static_state,sizeof(m->bounded_static_state)) ||
         !bounded_live_valid(m)))
        return STATUS_INVALID_DEVICE_STATE;
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
        if (!(r->mmu_flags&(A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_PM_FW_PROTECT)) && callstack>=r->gpu_va && n>=8u && callstack-r->gpu_va<=n-8u) ++owned;
    }
    if (owned!=1) return STATUS_INVALID_DEVICE_STATE;
    if (m->freelists_prepared && !freelists_valid(m,callstack)) return STATUS_INVALID_DEVICE_STATE;
    if (m->target_prepared) {
        const struct rogue_fwif_hwrtdata *d=m->target.data.cpu;
        const struct rogue_fwif_hwrtdata_common *c=m->target.common.cpu;
        uint64_t required_addr[7],required_len[7];
        if (!target_ranges_valid(m,&m->target_params,required_addr,required_len)) return STATUS_INVALID_DEVICE_STATE;
        for(i=0;i<7;++i) {
            unsigned matches=0;
            for(j=0;j<m->writable_count && j<A04_RENDER_MAX_WRITABLE;++j)
                if (m->writable[j].gpu_va==required_addr[i] && m->writable[j].size==required_len[i]) ++matches;
            if (matches!=1) return STATUS_INVALID_DEVICE_STATE;
        }
        if (m->target.data.size!=sizeof(*d) || m->target.common.size!=sizeof(*c) ||
            d->hwrt_data_common_fw_addr!=m->target.common.fw_addr ||
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
    for (i=0;i<count;++i) {
        status=a04_gpu_dma_sync(objects[i]->cpu,(ULONG)objects[i]->size,FALSE);
        if (!NT_SUCCESS(status)) { m->poisoned=TRUE;return status; }
    }
    return render_memory_publish(m,TRUE);
}
static void render_capture_trace(struct a04_windows_render_memory *m)
{
    struct a04_gpu_firmware_state *s=m->firmware;
    struct rogue_fwif_tracebuf *ctl; struct a04_shared_trace_summary result={0};
    uint32_t before[2],after[2]; size_t i,n=ROGUE_FW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS*sizeof(uint32_t);
    NTSTATUS status=STATUS_INVALID_DEVICE_STATE;
    m->trace_capture_valid=FALSE;
    if (!s || !s->memory.heap || s->runtime.next_free>A04_PVR_HEAP_SIZE ||
        !s->runtime.trace_control || !s->runtime.trace[0] || !s->runtime.trace[1] ||
        s->runtime.trace_control>s->runtime.next_free || sizeof(*ctl)>s->runtime.next_free-s->runtime.trace_control ||
        s->runtime.trace[0]>s->runtime.next_free || n>s->runtime.next_free-s->runtime.trace[0] ||
        s->runtime.trace[1]>s->runtime.next_free || n>s->runtime.next_free-s->runtime.trace[1] ||
        overlap(s->runtime.trace_control,sizeof(*ctl),s->runtime.trace[0],n) ||
        overlap(s->runtime.trace_control,sizeof(*ctl),s->runtime.trace[1],n) ||
        overlap(s->runtime.trace[0],n,s->runtime.trace[1],n)) goto done;
    ctl=(void *)((uint8_t *)s->memory.heap+s->runtime.trace_control);
    status=a04_gpu_dma_sync(ctl,sizeof(*ctl),TRUE); if (!NT_SUCCESS(status)) goto done;
    for(i=0;i<2;++i) before[i]=ctl->tracebuf[i].trace_pointer;
    for(i=0;i<2;++i) {
        uint32_t *words=(void *)((uint8_t *)s->memory.heap+s->runtime.trace[i]);
        status=a04_gpu_dma_sync(words,(ULONG)n,TRUE);if (!NT_SUCCESS(status)) goto done;
        a04_shared_trace_scan(words,ROGUE_FW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS,&result);
    }
    status=a04_gpu_dma_sync(ctl,sizeof(*ctl),TRUE);if (!NT_SUCCESS(status)) goto done;
    for(i=0;i<2;++i) after[i]=ctl->tracebuf[i].trace_pointer;
    m->trace_summary=result;
    m->trace_capture_changed=(before[0]!=after[0] || before[1]!=after[1]);
    m->trace_capture_valid=TRUE;
done:
    m->trace_capture_status=status;
}
static NTSTATUS render_prepare_pair(struct a04_windows_render_memory *m,
    const struct rogue_fwif_cmd_geom *geom,const struct rogue_fwif_cmd_frag *frag,
    const struct rogue_fwif_cmd_frag *final_frag,uint32_t job_id)
{
    struct a04_gpu_fw_arena arena={0};struct rogue_fwif_kccb_cmd kick={0},final_kick={0};
    struct a04_gpu_firmware_state *s;
    if (!m || !geom || !frag || !job_id || !m->ready || !m->firmware || !m->cpu ||
        !m->render_prepared || !m->target_prepared || !m->freelists_prepared || m->published ||
        m->poisoned || m->kick_prepared || m->kick_submitted || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    s=m->firmware;
    if (!s->processor_released || !s->firmware_started || s->queue_faulted || !s->memory.heap ||
        s->memory.adapter!=m->adapter || a04_gpu_fw_arena_init(&arena,s->memory.heap,A04_PVR_HEAP_SIZE,
            (uint64_t)s->memory.heap_logical.QuadPart,s->runtime.next_free)) return STATUS_INVALID_DEVICE_STATE;
    if (a04_gpu_prepare_render_pair(&arena,&m->render,&m->target.data,geom,frag,job_id,final_frag!=NULL,&kick))
        return STATUS_INVALID_DEVICE_STATE;
    if (final_frag && a04_gpu_prepare_final_fragment(&arena,&m->render,&m->target.data,final_frag,job_id,&final_kick)) {
        m->poisoned=TRUE;return STATUS_INVALID_DEVICE_STATE;
    }
    m->kick=kick;m->final_kick=final_kick;m->kick_partial=final_frag!=NULL;m->kick_prepared=TRUE;
    return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_prepare_pair(struct a04_windows_render_memory *m,
    const struct rogue_fwif_cmd_geom *geom,const struct rogue_fwif_cmd_frag *frag,uint32_t job_id)
{ return render_prepare_pair(m,geom,frag,NULL,job_id); }
NTSTATUS a04_windows_render_prepare_partial_pair(struct a04_windows_render_memory *m,
    const struct rogue_fwif_cmd_geom *geom,const struct rogue_fwif_cmd_frag *partial_frag,
    const struct rogue_fwif_cmd_frag *final_frag,uint32_t job_id)
{ if (!final_frag) return STATUS_INVALID_PARAMETER;return render_prepare_pair(m,geom,partial_frag,final_frag,job_id); }
NTSTATUS a04_windows_render_submit_pair(struct a04_windows_render_memory *m)
{
    NTSTATUS status;
    if (!m || !m->firmware || !m->published || m->poisoned || !m->kick_prepared || m->kick_submitted ||
        m->cleanup_complete || m->render.pair_prepared!=(m->kick_partial?3u:1u) || m->render.hwrt_fw!=m->target.data.fw_addr ||
        (m->kick_partial && m->final_kick.cmd_type!=ROGUE_FWIF_KCCB_CMD_KICK) ||
        m->kick.cmd_type!=ROGUE_FWIF_KCCB_CMD_COMBINED_GEOM_FRAG_KICK || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    if (!m->firmware->processor_released || !m->firmware->firmware_started || m->firmware->queue_faulted)
        return STATUS_INVALID_DEVICE_STATE;
    /* A failed or timed-out KCCB submission may already have reached firmware. */
    m->poisoned=TRUE;
    status=a04_gpu_firmware_submit_kick(m->firmware,&m->kick);
    if (!NT_SUCCESS(status)) return status;
    if (m->kick_partial) {
        status=a04_gpu_firmware_submit_kick(m->firmware,&m->final_kick);
        if (!NT_SUCCESS(status)) return status;
    }
    m->kick_submitted=TRUE;m->poisoned=FALSE;
    return STATUS_SUCCESS;
}
NTSTATUS a04_windows_render_collect_pair(struct a04_windows_render_memory *m)
{
    struct a04_gpu_firmware_state *s;struct rogue_fwif_cccb_ctl *gc,*fc;
    NTSTATUS status,observed;LARGE_INTEGER delay;unsigned i;
    const uint32_t geom_end=(uint32_t)(2u*sizeof(struct rogue_fwif_ccb_cmd_header)+sizeof(struct rogue_fwif_cmd_geom)+sizeof(struct rogue_fwif_ufo));
    const uint32_t base_frag_end=(uint32_t)(3u*sizeof(struct rogue_fwif_ccb_cmd_header)+sizeof(struct rogue_fwif_cmd_frag)+2u*sizeof(struct rogue_fwif_ufo));
    uint32_t frag_end,frag_fence;
    if (!m || !m->firmware || !m->published || m->poisoned || m->cleanup_complete || !m->target_prepared ||
        !m->freelists_prepared || !m->render_prepared || !m->kick_submitted ||
        m->render.pair_prepared!=(m->kick_partial?3u:1u) ||
        m->render.hwrt_fw!=m->target.data.fw_addr ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    frag_end=base_frag_end+(m->kick_partial?(uint32_t)(2u*sizeof(struct rogue_fwif_ccb_cmd_header)+sizeof(struct rogue_fwif_cmd_frag)+sizeof(struct rogue_fwif_ufo)):0u);
    frag_fence=m->kick_partial?2u:1u;
    if (m->render.frag_end!=frag_end) return STATUS_INVALID_PARAMETER;
    s=m->firmware;
    if (!s->processor_released || !s->firmware_started || s->queue_faulted ||
        !owned_fw(m,&m->render.context,sizeof(struct rogue_fwif_fwrendercontext)) ||
        !owned_fw(m,&m->render.geom_control,sizeof(struct rogue_fwif_cccb_ctl)) ||
        !owned_fw(m,&m->render.frag_control,sizeof(struct rogue_fwif_cccb_ctl)) ||
        !owned_fw(m,&m->render.geom_fence,sizeof(uint32_t)) || !owned_fw(m,&m->render.frag_fence,sizeof(uint32_t)) ||
        !owned_fw(m,&m->target.data,sizeof(struct rogue_fwif_hwrtdata)) ||
        !owned_fw(m,&m->freelists[0],sizeof(struct rogue_fwif_freelist)) ||
        !owned_fw(m,&m->freelists[1],sizeof(struct rogue_fwif_freelist)) ||
        m->render.geom_context_fw!=m->render.context.fw_addr+offsetof(struct rogue_fwif_fwrendercontext,geom_context) ||
        m->render.frag_context_fw!=m->render.context.fw_addr+offsetof(struct rogue_fwif_fwrendercontext,frag_context)) return STATUS_INVALID_DEVICE_STATE;
    gc=m->render.geom_control.cpu;fc=m->render.frag_control.cpu;delay.QuadPart=-100000;
    /* After observing a possibly submitted job, any failure is ambiguous. */
    m->poisoned=TRUE;
    for(i=0;i<200u;++i) {
        status=a04_gpu_dma_sync(gc,(ULONG)m->render.geom_control.size,TRUE);if(!NT_SUCCESS(status)) return status;
        status=a04_gpu_dma_sync(fc,(ULONG)m->render.frag_control.size,TRUE);if(!NT_SUCCESS(status)) return status;
        status=a04_gpu_dma_sync(m->render.geom_fence.cpu,sizeof(uint32_t),TRUE);if(!NT_SUCCESS(status)) return status;
        status=a04_gpu_dma_sync(m->render.frag_fence.cpu,sizeof(uint32_t),TRUE);if(!NT_SUCCESS(status)) return status;
        if (*(volatile uint32_t *)m->render.geom_fence.cpu==1 && *(volatile uint32_t *)m->render.frag_fence.cpu==frag_fence &&
            *(volatile uint32_t *)&gc->read_offset==geom_end && *(volatile uint32_t *)&fc->read_offset==frag_end) break;
        status=KeDelayExecutionThread(KernelMode,FALSE,&delay);if(!NT_SUCCESS(status)) return status;
    }
    if (m->trace_capture_enabled) render_capture_trace(m);
    if(i==200u) return STATUS_IO_TIMEOUT;
    status=a04_gpu_dma_sync(m->cpu,m->length,TRUE);if(!NT_SUCCESS(status)) return status;
    m->completion_observed=TRUE;
    observed=a04_windows_render_check_bounded_states(m);
    status=a04_gpu_firmware_cleanup(s,ROGUE_FWIF_CLEANUP_FWCOMMONCONTEXT,m->render.geom_context_fw);if(!NT_SUCCESS(status)) return status;
    status=a04_gpu_firmware_cleanup(s,ROGUE_FWIF_CLEANUP_FWCOMMONCONTEXT,m->render.frag_context_fw);if(!NT_SUCCESS(status)) return status;
    status=a04_gpu_firmware_cleanup(s,ROGUE_FWIF_CLEANUP_HWRTDATA,m->target.data.fw_addr);if(!NT_SUCCESS(status)) return status;
    status=a04_gpu_firmware_cleanup(s,ROGUE_FWIF_CLEANUP_FREELIST,m->freelists[0].fw_addr);if(!NT_SUCCESS(status)) return status;
    status=a04_gpu_firmware_cleanup(s,ROGUE_FWIF_CLEANUP_FREELIST,m->freelists[1].fw_addr);if(!NT_SUCCESS(status)) return status;
    m->cleanup_complete=TRUE;m->poisoned=FALSE;return observed;
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
