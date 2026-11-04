/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "windows_render_memory.h"
#include "gpu_mmu.h"
#include "pvr_rogue_fwif.h"
static unsigned allocated,freed,cleans,invalidations,fail_sync,fail_sync_at,fail_inv,expect_staged;
static void *alloc_buf(PDMA_ADAPTER a,ULONG n,PHYSICAL_ADDRESS *pa,BOOLEAN cache)
{ (void)a;(void)cache;pa->QuadPart=0x10000000u;++allocated;return aligned_alloc(4096,n); }
static void free_buf(PDMA_ADAPTER a,ULONG n,PHYSICAL_ADDRESS pa,void *p,BOOLEAN cache)
{ (void)a;(void)n;(void)pa;(void)cache;++freed;free(p); }
unsigned KeGetCurrentIrql(void) { return PASSIVE_LEVEL; }
NTSTATUS a04_gpu_dma_sync(void *p,ULONG n,BOOLEAN rd)
{ assert(p && n && !rd);if(expect_staged && n==20u*4096u) { const unsigned char *b=(const unsigned char *)p+A04_RENDER_PAYLOAD_OFFSET+4u*4096u;
  assert(b[0]==0x40 && b[1]==0x50 && b[4]==8 && b[32]==2 && b[35]==0xf4); }
  ++cleans;return (fail_sync || cleans==fail_sync_at) ? STATUS_DEVICE_CONFIGURATION_ERROR : STATUS_SUCCESS; }
NTSTATUS a04_gpu_firmware_invalidate_mmu(struct a04_gpu_firmware_state *s)
{ assert(s);++invalidations;return fail_inv ? STATUS_IO_TIMEOUT : STATUS_SUCCESS; }
int main(void)
{
    DMA_OPERATIONS ops={alloc_buf,free_buf};struct fake_adapter adapter={&ops};
    struct a04_gpu_firmware_state s={0};struct a04_windows_render_memory m={0};
    struct rogue_fwif_geom_registers_caswitch static_state={0};
    struct a04_windows_render_map maps[]={{0x80000b6000ULL,0,4,0,A04_GPU_MMU_SLC_BYPASS},{0xda00002000ULL,4,1,1,A04_GPU_MMU_READ_ONLY},
        {0xe000005000ULL,5,1,2,A04_GPU_MMU_READ_ONLY}};
    s.memory.adapter=&adapter;s.memory.heap=aligned_alloc(4096,A04_PVR_HEAP_SIZE);assert(s.memory.heap);
    s.memory.heap_logical.QuadPart=0x80000000u;s.runtime.next_free=4096;
    s.processor_released=TRUE;s.firmware_started=TRUE;
    assert(a04_windows_render_memory_create(&m,&s,6,maps,3)==STATUS_SUCCESS);
    assert(allocated==1 && m.cpu && m.length==20u*4096u);
    for (unsigned i=A04_RENDER_PAYLOAD_OFFSET;i<m.length;++i) assert(!((unsigned char *)m.cpu)[i]);
    assert(a04_windows_render_stage_fragment_pds(&m,0xda00002000ULL,0xe000005040ULL,UINT64_C(1)<<35)==STATUS_SUCCESS);
    unsigned char *p=(unsigned char *)m.cpu+A04_RENDER_PAYLOAD_OFFSET+4u*4096u;
    assert(p[0]==0x40 && p[1]==0x50 && p[4]==8 && p[32]==2 && p[35]==0xf4);
    for (unsigned i=8;i<32;++i) assert(!p[i]);
    for (unsigned i=36;i<48;++i) assert(!p[i]);
    assert(a04_windows_render_stage_fragment_pds(&m,0xda00002ff0ULL,0xe000005040ULL,0)==STATUS_INVALID_PARAMETER);
    assert(a04_windows_render_stage_fragment_pds(&m,0x80000b6000ULL,0xe000005040ULL,0)==STATUS_INVALID_PARAMETER);
    assert(a04_windows_render_stage_fragment_pds(&m,0xda00002000ULL,0x80000b6000ULL,0)==STATUS_INVALID_PARAMETER);
    assert(a04_windows_render_stage_fragment_pds(&m,0xda00002000ULL,0xe000005ff0ULL,0)==STATUS_INVALID_PARAMETER);
    assert(a04_windows_render_stage_fragment_pds(&m,0xda00002000ULL,0xe000005040ULL,UINT64_C(1)<<33)==STATUS_INVALID_PARAMETER);
    assert(p[0]==0x40 && p[4]==8 && p[32]==2 && p[35]==0xf4);
    assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_DEVICE_BUSY && !freed);
    assert(a04_windows_render_memory_publish(&m)==STATUS_INVALID_PARAMETER && cleans==0);
    assert(a04_windows_render_memory_bind_vm(&m)==STATUS_SUCCESS);
    assert(((struct rogue_fwif_fwmemcontext *)m.vm.cpu)->pc_dev_paddr==0x10000000u);
    assert(a04_windows_render_memory_bind_vm(&m)==STATUS_INVALID_PARAMETER);
    ((struct rogue_fwif_fwmemcontext *)m.vm.cpu)->pc_dev_paddr+=4096;
    assert(a04_windows_render_memory_publish(&m)==STATUS_INVALID_DEVICE_STATE && cleans==0);
    ((struct rogue_fwif_fwmemcontext *)m.vm.cpu)->pc_dev_paddr-=4096;
    assert(a04_windows_render_prepare_context(&m,1,0xda00002000ULL,4,&static_state)==STATUS_INVALID_PARAMETER);
    assert(a04_windows_render_prepare_context(&m,1,0x80000b6000ULL,4,&static_state)==STATUS_SUCCESS);
    { struct rogue_fwif_fwrendercontext *ctx=m.render.context.cpu;
      assert(ctx->geom_context.fw_mem_context_fw_addr==m.vm.fw_addr);
      assert(ctx->frag_context.fw_mem_context_fw_addr==m.vm.fw_addr); }
    assert(a04_windows_render_prepare_context(&m,1,0x80000b6000ULL,4,&static_state)==STATUS_INVALID_PARAMETER);
    assert(a04_windows_render_memory_publish(&m)==STATUS_INVALID_PARAMETER && cleans==0);
    expect_staged=1;assert(a04_windows_render_publish_context(&m)==STATUS_SUCCESS && m.published && !m.poisoned);expect_staged=0;
    assert(a04_windows_render_stage_fragment_pds(&m,0xda00002000ULL,0xe000005040ULL,0)==STATUS_INVALID_PARAMETER);
    assert(cleans==11 && invalidations==1);
    assert(a04_windows_render_memory_publish(&m)==STATUS_INVALID_PARAMETER);
    s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,FALSE)==STATUS_DEVICE_BUSY && !freed);
    assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS && freed==1);
    s.processor_released=TRUE;assert(a04_windows_render_memory_create(&m,&s,6,maps,3)==STATUS_SUCCESS);
    assert(a04_windows_render_memory_bind_vm(&m)==STATUS_SUCCESS);
    fail_inv=1;assert(a04_windows_render_memory_publish(&m)==STATUS_IO_TIMEOUT && m.poisoned && !m.published);
    assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_DEVICE_BUSY && freed==1);
    s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS && freed==2);
    s.processor_released=TRUE;fail_inv=0;
    assert(a04_windows_render_memory_create(&m,&s,6,maps,3)==STATUS_SUCCESS);
    assert(a04_windows_render_memory_bind_vm(&m)==STATUS_SUCCESS);
    { unsigned inv_before=invalidations;fail_sync_at=cleans+2;
      assert(a04_windows_render_memory_publish(&m)==STATUS_DEVICE_CONFIGURATION_ERROR);
      assert(m.poisoned && !m.published && invalidations==inv_before); }
    assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_DEVICE_BUSY);
    s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS && freed==3);
    fail_sync_at=0;
    s.processor_released=TRUE;
    assert(a04_windows_render_memory_create(&m,&s,6,maps,3)==STATUS_SUCCESS);
    assert(a04_windows_render_memory_bind_vm(&m)==STATUS_SUCCESS);
    assert(a04_windows_render_prepare_context(&m,2,0x80000b6000ULL,4,&static_state)==STATUS_SUCCESS);
    { void *saved=m.render.frag_ring.cpu;unsigned before=cleans;
      m.render.frag_ring.cpu=(uint8_t *)saved+4;
      assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && cleans==before && !m.poisoned);
      m.render.frag_ring.cpu=saved; }
    { struct rogue_fwif_fwrendercontext *ctx=m.render.context.cpu;unsigned before=cleans;
      uint32_t saved=ctx->frag_context.fw_mem_context_fw_addr;
      ctx->frag_context.fw_mem_context_fw_addr+=64;
      assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && cleans==before && !m.poisoned);
      ctx->frag_context.fw_mem_context_fw_addr=saved; }
    { struct rogue_fwif_geom_ctx_state *geom=m.render.geom_state.cpu;unsigned before=cleans;
      uint64_t saved=geom->geom_core[0].geom_reg_vdm_call_stack_pointer_init;
      geom->geom_core[0].geom_reg_vdm_call_stack_pointer_init=0xda00002000ULL;
      assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && cleans==before && !m.poisoned);
      geom->geom_core[0].geom_reg_vdm_call_stack_pointer_init=saved; }
    { unsigned inv_before=invalidations;fail_sync_at=cleans+5;
      assert(a04_windows_render_publish_context(&m)==STATUS_DEVICE_CONFIGURATION_ERROR);
      assert(m.poisoned && !m.published && invalidations==inv_before); }
    assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_DEVICE_BUSY);
    s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS && freed==4);
    fail_sync_at=0;
    s.processor_released=TRUE;maps[1].payload_page=0;
    assert(a04_windows_render_memory_create(&m,&s,5,maps,2)==STATUS_INVALID_PARAMETER && allocated==4);
    maps[1].payload_page=4;maps[1].gpu_va=maps[0].gpu_va;
    assert(a04_windows_render_memory_create(&m,&s,5,maps,2)==STATUS_INVALID_PARAMETER && allocated==4);
    maps[0].pages=513;
    assert(a04_windows_render_memory_create(&m,&s,513,maps,1)==STATUS_DEVICE_CONFIGURATION_ERROR);
    assert(m.cpu && !m.ready);
    assert(a04_windows_render_memory_publish(&m)==STATUS_INVALID_PARAMETER);
    s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS);
    {
      struct a04_windows_render_freelist_layout fl[2]={{0x80000b6100ULL,6,64,1},{0x80000b7000ULL,70,1024,2}};
      struct a04_render_hwrt_params hw={0};
      uint32_t *table;unsigned before,inv_before;
      maps[0].pages=4;maps[1].gpu_va=0xda00002000ULL;s.processor_released=TRUE;
      assert(a04_windows_render_memory_create(&m,&s,1094,maps,3)==STATUS_SUCCESS);
      assert(a04_windows_render_memory_bind_vm(&m)==STATUS_SUCCESS);
      assert(a04_windows_render_prepare_context(&m,3,0x80000b6000ULL,4,&static_state)==STATUS_SUCCESS);
      m.maps[0].mmu_flags&=~A04_GPU_MMU_SLC_BYPASS;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      m.maps[0].mmu_flags|=A04_GPU_MMU_SLC_BYPASS;
      fl[0].page_count=52;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      fl[0].page_count=64;fl[0].table_gpu+=16;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      fl[0].table_gpu-=16;
      fl[1].first_payload_page=69;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      fl[1].first_payload_page=70;fl[0].first_payload_page=5;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      fl[0].first_payload_page=6;fl[0].table_gpu=0x80000b6000ULL;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      fl[0].table_gpu=0x80000b6100ULL;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_SUCCESS);
      table=(uint32_t *)((uint8_t *)m.cpu+A04_RENDER_PAYLOAD_OFFSET+0x100u);
      assert(table[0]==(uint32_t)((0x10000000u+A04_RENDER_PAYLOAD_OFFSET+6u*4096u)>>12));
      assert(table[63]==table[0]+63u);
      assert(((struct rogue_fwif_freelist *)m.freelists[0].cpu)->current_pages==64);
      hw.width=64;hw.height=64;hw.tpc_size=128;hw.region_header_size=16;
      hw.tpc_gpu=0x80000b8100ULL;hw.vheap_gpu=0x80000b8200ULL;
      hw.pm_mlist_gpu=0x80000b9000ULL;hw.region_gpu=0x80000b8900ULL;
      hw.vheap_gpu=hw.tpc_gpu;
      assert(a04_windows_render_prepare_target(&m,&hw)==STATUS_INVALID_DEVICE_STATE);
      hw.vheap_gpu=0x80000b8200ULL;
      assert(a04_windows_render_prepare_target(&m,&hw)==STATUS_SUCCESS);
      assert(((struct rogue_fwif_hwrtdata *)m.target.data.cpu)->freelists_fw_addr[0]==m.freelists[0].fw_addr);
      table[17]++;
      before=cleans;inv_before=invalidations;
      assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && cleans==before && invalidations==inv_before);
      table[17]--;
      { struct rogue_fwif_hwrtdata *target=m.target.data.cpu;
        target->vheap_table_dev_addr+=16;
        assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && cleans==before && invalidations==inv_before);
        target->vheap_table_dev_addr-=16; }
      before=cleans;inv_before=invalidations;
      assert(a04_windows_render_publish_context(&m)==STATUS_NOT_SUPPORTED && cleans==before && invalidations==inv_before && !m.published);
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS);
    }
    free(s.memory.heap);
    return 0;
}
