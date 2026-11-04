/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_render_memory.h"
#if A04_GE8300_RENDER_EXPERIMENT
#include "gpu_mmu.h"
#include "render_pds_data.h"
#include "pvr_rogue_fwif.h"
#define PAGE 4096u
#define TABLE_PAGES 14u
#define MAX_PAYLOAD_PAGES 4096u
#define FRAGMENT_USC_BYTES 360u
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<=b ? b-a<n : a-b<m; }
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
    struct a04_gpu_fw_object *objects[9];struct rogue_fwif_fwrendercontext *ctx;
    struct rogue_fwif_geom_ctx_state *geom;uint64_t callstack;int owned=0;size_t i,j;NTSTATUS status;
    if (!m || !m->render_prepared || !m->ready || !m->firmware || !m->cpu || !m->vm.cpu ||
        m->published || m->poisoned || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    objects[0]=&m->render.context;objects[1]=&m->render.geom_control;objects[2]=&m->render.geom_ring;
    objects[3]=&m->render.geom_state;objects[4]=&m->render.geom_fence;objects[5]=&m->render.frag_control;
    objects[6]=&m->render.frag_ring;objects[7]=&m->render.frag_state;objects[8]=&m->render.frag_fence;
    for (i=0;i<9;++i) {
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
    for (i=0;i<9;++i) {
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
