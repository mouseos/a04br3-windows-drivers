/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "windows_render_memory.h"
#include "windows_render_diagnostic.h"
#include "gpu_mmu.h"
#include "pvr_rogue_fwif.h"
#include "pvr_rogue_fwif_client.h"
static unsigned allocated,freed,cleans,invalidations,fail_sync,fail_sync_at,fail_inv,expect_staged;
static unsigned cleanup_calls,cleanup_fail_at;
static unsigned kick_calls,kick_fail,kick_fail_at;
static struct a04_windows_render_diagnostic *diagnostic_mock;
static unsigned diagnostic_wrong_pixels;
static void *alloc_buf(PDMA_ADAPTER a,ULONG n,PHYSICAL_ADDRESS *pa,BOOLEAN cache)
{ (void)a;(void)cache;pa->QuadPart=0x10000000u;++allocated;return aligned_alloc(4096,n); }
static void free_buf(PDMA_ADAPTER a,ULONG n,PHYSICAL_ADDRESS pa,void *p,BOOLEAN cache)
{ (void)a;(void)n;(void)pa;(void)cache;++freed;free(p); }
unsigned KeGetCurrentIrql(void) { return PASSIVE_LEVEL; }
NTSTATUS a04_gpu_dma_sync(void *p,ULONG n,BOOLEAN rd)
{ assert(p && n);if(expect_staged && !rd && n==20u*4096u) { const unsigned char *b=(const unsigned char *)p+A04_RENDER_PAYLOAD_OFFSET+4u*4096u;
  assert(b[0]==0x40 && b[1]==0x50 && b[4]==8 && b[32]==2 && b[35]==0xf4); }
  ++cleans;return (fail_sync || cleans==fail_sync_at) ? STATUS_DEVICE_CONFIGURATION_ERROR : STATUS_SUCCESS; }
NTSTATUS a04_gpu_firmware_invalidate_mmu(struct a04_gpu_firmware_state *s)
{ assert(s);++invalidations;return fail_inv ? STATUS_IO_TIMEOUT : STATUS_SUCCESS; }
NTSTATUS KeDelayExecutionThread(int mode,BOOLEAN alert,const LARGE_INTEGER *delay)
{ (void)mode;(void)alert;(void)delay;return STATUS_SUCCESS; }
NTSTATUS a04_gpu_firmware_cleanup(struct a04_gpu_firmware_state *s,unsigned type,uint32_t address)
{ (void)type;assert(s && address);++cleanup_calls;return cleanup_calls==cleanup_fail_at ? STATUS_DEVICE_BUSY : STATUS_SUCCESS; }
NTSTATUS a04_gpu_firmware_submit_kick(struct a04_gpu_firmware_state *s,const struct rogue_fwif_kccb_cmd *kick)
{ assert(s && kick && (kick->cmd_type==ROGUE_FWIF_KCCB_CMD_COMBINED_GEOM_FRAG_KICK || kick->cmd_type==ROGUE_FWIF_KCCB_CMD_KICK));++kick_calls;
  if (diagnostic_mock && kick->cmd_type==ROGUE_FWIF_KCCB_CMD_KICK) {
    struct a04_windows_render_memory *m=&diagnostic_mock->memory;
    uint8_t *p=(uint8_t *)m->cpu+A04_RENDER_PAYLOAD_OFFSET+65u*4096u;unsigned x,y,i;
    *(uint32_t *)m->render.geom_fence.cpu=1;*(uint32_t *)m->render.frag_fence.cpu=2;
    ((struct rogue_fwif_cccb_ctl *)m->render.geom_control.cpu)->read_offset=(uint32_t)(2u*sizeof(struct rogue_fwif_ccb_cmd_header)+sizeof(struct rogue_fwif_cmd_geom)+sizeof(struct rogue_fwif_ufo));
    ((struct rogue_fwif_cccb_ctl *)m->render.frag_control.cpu)->read_offset=m->render.frag_end;
    for(y=0;y<64;++y) for(x=0;x<64;++x) { unsigned tx=diagnostic_wrong_pixels==2 ? 63-x : diagnostic_wrong_pixels==3 ? y : x; unsigned ty=diagnostic_wrong_pixels==3 ? x : y; int inside=tx>=8 && tx<49 && ty>=8 && tx+ty<63; i=y*64u+x;
      if(diagnostic_wrong_pixels==4) { memset(p+4*i,0,4);continue; }
      p[4*i]=diagnostic_wrong_pixels==1?0:(inside?0xff:0x22);p[4*i+1]=inside?0:0x44;p[4*i+2]=inside?0xff:0x66;p[4*i+3]=0xff; }
  }
  return kick_fail || kick_calls==kick_fail_at ? STATUS_IO_TIMEOUT : STATUS_SUCCESS; }
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
    unsigned char *p=(unsigned char *)m.cpu+A04_RENDER_PAYLOAD_OFFSET+4u*4096u;
    p[47]=1;
    assert(a04_windows_render_stage_fragment_pds(&m,0xda00002000ULL,0xe000005040ULL,UINT64_C(1)<<35)==STATUS_INVALID_DEVICE_STATE);
    assert(!p[0] && !m.staged_count);p[47]=0;
    assert(a04_windows_render_stage_fragment_pds(&m,0xda00002000ULL,0xe000005040ULL,UINT64_C(1)<<35)==STATUS_SUCCESS);
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
      struct a04_windows_render_freelist_layout fl[2]={{0x80000ba000ULL,8,64,1},{0x80000bb000ULL,72,1024,2}};
      struct a04_windows_render_map pm_maps[4];
      struct a04_render_hwrt_params hw={0};
      uint32_t *table;unsigned before,inv_before;
      maps[0].pages=4;maps[1].gpu_va=0xda00002000ULL;s.processor_released=TRUE;
      pm_maps[0]=maps[0];pm_maps[1]=maps[1];pm_maps[2]=maps[2];
      pm_maps[3]=(struct a04_windows_render_map){0x80000ba000ULL,6,2,0,A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_PM_FW_PROTECT};
      pm_maps[3].gpu_va+=0x200000u;
      assert(a04_windows_render_memory_create(&m,&s,1096,pm_maps,4)==STATUS_INVALID_PARAMETER);
      pm_maps[3].gpu_va-=0x200000u;
      assert(a04_windows_render_memory_create(&m,&s,1096,pm_maps,4)==STATUS_SUCCESS);
      assert(a04_windows_render_memory_bind_vm(&m)==STATUS_SUCCESS);
      assert(a04_windows_render_prepare_context(&m,3,0x80000b6000ULL,4,&static_state)==STATUS_SUCCESS);
      m.maps[3].mmu_flags&=~A04_GPU_MMU_SLC_BYPASS;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      m.maps[3].mmu_flags|=A04_GPU_MMU_SLC_BYPASS;
      m.maps[3].mmu_flags&=~A04_GPU_MMU_PM_FW_PROTECT;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      m.maps[3].mmu_flags|=A04_GPU_MMU_PM_FW_PROTECT;
      fl[0].page_count=52;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      fl[0].page_count=64;fl[0].table_gpu+=16;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      fl[0].table_gpu-=16;
      fl[1].first_payload_page=71;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      fl[1].first_payload_page=72;fl[0].first_payload_page=7;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      fl[0].first_payload_page=8;fl[0].table_gpu=0x80000b6000ULL;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      fl[0].table_gpu=0x80000ba000ULL;
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_SUCCESS);
      table=(uint32_t *)((uint8_t *)m.cpu+A04_RENDER_PAYLOAD_OFFSET+6u*4096u);
      assert(table[0]==(uint32_t)((0x10000000u+A04_RENDER_PAYLOAD_OFFSET+8u*4096u)>>12));
      assert(table[63]==table[0]+63u);
      assert(((struct rogue_fwif_freelist *)m.freelists[0].cpu)->current_pages==64);
      hw.width=64;hw.height=64;hw.tpc_size=128;hw.region_header_size=16;
      hw.tpc_gpu=0x80000b8100ULL;hw.vheap_gpu=0x80000b8200ULL;
      hw.pm_mlist_gpu=0x80000b9000ULL;hw.region_gpu=0x80000b8900ULL;
      hw.vheap_gpu=hw.tpc_gpu;
      assert(a04_windows_render_prepare_target(&m,&hw)==STATUS_INVALID_DEVICE_STATE);
      hw.vheap_gpu=0x80000b8200ULL;
      m.staged[0].gpu_va=hw.tpc_gpu;m.staged[0].size=16;m.staged_count=1;
      assert(a04_windows_render_prepare_target(&m,&hw)==STATUS_INVALID_DEVICE_STATE && !m.target_prepared);
      m.staged_count=0;
      assert(a04_windows_render_prepare_target(&m,&hw)==STATUS_SUCCESS);
      assert(m.writable_count==7);
      assert(((struct rogue_fwif_hwrtdata *)m.target.data.cpu)->freelists_fw_addr[0]==m.freelists[0].fw_addr);
      table[17]++;
      before=cleans;inv_before=invalidations;
      assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && cleans==before && invalidations==inv_before);
      table[17]--;
      { uint64_t saved=m.writable[0].size;m.writable[0].size=64;
        assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && cleans==before && invalidations==inv_before);
        m.writable[0].size=saved; }
      { struct rogue_fwif_hwrtdata *target=m.target.data.cpu;
        target->vheap_table_dev_addr+=16;
        assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && cleans==before && invalidations==inv_before);
        target->vheap_table_dev_addr-=16; }
      before=cleans;inv_before=invalidations;
      { uint64_t *pt=(uint64_t *)((uint8_t *)m.cpu+7u*4096u);size_t index=(size_t)((fl[0].table_gpu>>12)&511u);
        assert((pt[index]&A04_GPU_MMU_PM_FW_PROTECT)!=0);
        assert(!(pt[(0x80000b6000ULL>>12)&511u]&A04_GPU_MMU_PM_FW_PROTECT));
        pt[index]^=A04_GPU_MMU_PM_FW_PROTECT;
        assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && !m.published && invalidations==inv_before);
        pt[index]^=A04_GPU_MMU_PM_FW_PROTECT; }
      { uint64_t *pt=(uint64_t *)((uint8_t *)m.cpu+7u*4096u);size_t unused=400;
        assert(!pt[unused]);pt[unused]=0x12345001u;
        assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && !m.published && invalidations==inv_before);
        pt[unused]=0; }
      { uint64_t *pd=(uint64_t *)((uint8_t *)m.cpu+6u*4096u);size_t unused=400;
        assert(!pd[unused]);pd[unused]=0x12345001u;
        assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && !m.published && invalidations==inv_before);
        pd[unused]=0; }
      { uint8_t *zero=(uint8_t *)m.cpu+5u*4096u;zero[3]=1;
        assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && !m.published && invalidations==inv_before);
        zero[3]=0; }
      before=cleans;inv_before=invalidations;
      { struct rogue_fwif_cmd_geom geom={0};struct rogue_fwif_cmd_frag frag={0};
        assert(a04_windows_render_prepare_partial_pair(&m,&geom,&frag,NULL,1)==STATUS_INVALID_PARAMETER);
        assert(a04_windows_render_prepare_partial_pair(&m,&geom,&frag,&frag,1)==STATUS_SUCCESS);
        assert(a04_windows_render_prepare_pair(&m,&geom,&frag,1)==STATUS_INVALID_PARAMETER);
        assert(a04_windows_render_submit_pair(&m)==STATUS_INVALID_PARAMETER && !kick_calls); }
      assert(a04_windows_render_publish_context(&m)==STATUS_SUCCESS && cleans==before+15 && invalidations==inv_before+1 && m.published);
      assert(a04_windows_render_prepare_freelists(&m,fl)==STATUS_INVALID_PARAMETER);
      assert(a04_windows_render_collect_pair(&m)==STATUS_INVALID_PARAMETER);
      { struct a04_windows_render_memory copy=m;kick_fail=1;
        assert(a04_windows_render_submit_pair(&copy)==STATUS_IO_TIMEOUT && copy.poisoned && !copy.kick_submitted);
        assert(a04_windows_render_submit_pair(&copy)==STATUS_INVALID_PARAMETER);kick_fail=0; }
      { struct a04_windows_render_memory copy=m;kick_fail_at=3;
        assert(a04_windows_render_submit_pair(&copy)==STATUS_IO_TIMEOUT && copy.poisoned && !copy.kick_submitted);
        assert(kick_calls==3 && a04_windows_render_submit_pair(&copy)==STATUS_INVALID_PARAMETER);kick_fail_at=0; }
      assert(a04_windows_render_submit_pair(&m)==STATUS_SUCCESS && m.kick_submitted && kick_calls==5);
      assert(a04_windows_render_submit_pair(&m)==STATUS_INVALID_PARAMETER && kick_calls==5);
      { uint32_t saved_next=s.runtime.next_free;
        uint32_t *trace;
        s.runtime.trace_control=0x200000;s.runtime.trace[0]=0x201000;s.runtime.trace[1]=0x20d000;s.runtime.next_free=0x219000;
        memset((uint8_t *)s.memory.heap+0x200000,0,0x19000);
        trace=(uint32_t *)((uint8_t *)s.memory.heap+s.runtime.trace[0]);
        trace[10]=A04_SHARED_TRACE_ID;trace[13]=64;trace[14]=256;
        { struct a04_windows_render_memory copy=m;copy.trace_capture_enabled=TRUE;
          assert(a04_windows_render_collect_pair(&copy)==STATUS_IO_TIMEOUT && copy.poisoned);
          assert(copy.trace_capture_valid && copy.trace_capture_status==STATUS_SUCCESS && !copy.trace_capture_changed);
          assert(copy.trace_summary.candidates==1 && copy.trace_summary.store_mask==2 && copy.trace_summary.load_mask==16); }
        { struct a04_windows_render_memory copy=m;copy.trace_capture_enabled=TRUE;s.runtime.trace[1]=s.runtime.trace[0];
          assert(a04_windows_render_collect_pair(&copy)==STATUS_IO_TIMEOUT && copy.poisoned);
          assert(!copy.trace_capture_valid && copy.trace_capture_status==STATUS_INVALID_DEVICE_STATE); }
        s.runtime.trace_control=0;s.runtime.trace[0]=s.runtime.trace[1]=0;s.runtime.next_free=saved_next;
      }
      { struct a04_windows_render_memory copy=m;
        assert(a04_windows_render_collect_pair(&copy)==STATUS_IO_TIMEOUT && copy.poisoned && !cleanup_calls);
        assert(a04_windows_render_memory_free(&copy,TRUE)==STATUS_DEVICE_BUSY); }
      *(uint32_t *)m.render.geom_fence.cpu=1;*(uint32_t *)m.render.frag_fence.cpu=1;
      ((struct rogue_fwif_cccb_ctl *)m.render.geom_control.cpu)->read_offset=(uint32_t)(2u*sizeof(struct rogue_fwif_ccb_cmd_header)+sizeof(struct rogue_fwif_cmd_geom)+sizeof(struct rogue_fwif_ufo));
      ((struct rogue_fwif_cccb_ctl *)m.render.frag_control.cpu)->read_offset=m.render.frag_end;
      { struct a04_windows_render_memory copy=m;
        assert(a04_windows_render_collect_pair(&copy)==STATUS_IO_TIMEOUT && copy.poisoned && !cleanup_calls); }
      *(uint32_t *)m.render.frag_fence.cpu=2;
      { struct a04_windows_render_memory copy=m;cleanup_fail_at=3;
        assert(a04_windows_render_collect_pair(&copy)==STATUS_DEVICE_BUSY && copy.poisoned && !copy.cleanup_complete && cleanup_calls==3);
        assert(a04_windows_render_memory_free(&copy,TRUE)==STATUS_DEVICE_BUSY);cleanup_calls=0;cleanup_fail_at=0; }
      { struct a04_windows_render_memory copy=m;copy.bounded_state_count=1;copy.bounded_states[0].gpu_va=0x80000b6000ULL;
        assert(a04_windows_render_collect_pair(&copy)==STATUS_DATA_ERROR && copy.cleanup_complete && !copy.poisoned && cleanup_calls==5);
        assert(copy.completion_observed && copy.bounded_states_checked && !copy.bounded_states[0].valid);cleanup_calls=0; }
      assert(a04_windows_render_collect_pair(&m)==STATUS_SUCCESS && m.cleanup_complete && cleanup_calls==5 && !m.poisoned);
      assert(a04_windows_render_collect_pair(&m)==STATUS_INVALID_PARAMETER);
      s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS);
    }
    {
      struct a04_windows_render_map ptmaps[]={{0xda00010000ULL,0,1,0,0},
          {0xda00011000ULL,1,1,0,A04_GPU_MMU_READ_ONLY}};
      uint8_t *output;s.processor_released=TRUE;
      assert(a04_windows_render_memory_create(&m,&s,2,ptmaps,2)==STATUS_SUCCESS);
      output=(uint8_t *)m.cpu+A04_RENDER_PAYLOAD_OFFSET+4096u;
      assert(a04_windows_render_stage_context_pt(&m,0xda00010000ULL,0xda00011000ULL,0xda00011020ULL,
          0xda00011040ULL,0xda00011040ULL)==STATUS_INVALID_PARAMETER);
      for(unsigned i=0;i<128;++i) assert(!output[i]);
      output[127]=1;
      assert(a04_windows_render_stage_context_pt(&m,0xda00010000ULL,0xda00011000ULL,0xda00011020ULL,
          0xda00011040ULL,0xda00011060ULL)==STATUS_INVALID_DEVICE_STATE);
      assert(!m.writable_count && !m.staged_count && !output[0]);output[127]=0;
      ((uint8_t *)m.cpu)[A04_RENDER_PAYLOAD_OFFSET+127]=1;
      assert(a04_windows_render_stage_context_pt(&m,0xda00010000ULL,0xda00011000ULL,0xda00011020ULL,
          0xda00011040ULL,0xda00011060ULL)==STATUS_INVALID_DEVICE_STATE);
      ((uint8_t *)m.cpu)[A04_RENDER_PAYLOAD_OFFSET+127]=0;
      assert(a04_windows_render_stage_context_pt(&m,0xda00010000ULL,0xda00011000ULL,0xda00011020ULL,
          0xda00011040ULL,0xda00011060ULL)==STATUS_SUCCESS);
      assert(m.writable_count==1);
      assert(output[32]==0 && output[35]==0xd0 && output[36]==1 && output[39]==0xd0);
      assert(output[96]==0x10 && output[99]==0xd3 && output[100]==0 && output[103]==0xd0);
      for(unsigned i=52;i<64;++i) assert(!output[i]);
      for(unsigned i=120;i<128;++i) assert(!output[i]);
      assert(a04_windows_render_memory_bind_vm(&m)==STATUS_SUCCESS);
      assert(a04_windows_render_memory_publish(&m)==STATUS_SUCCESS);
      assert(a04_windows_render_stage_context_pt(&m,0xda00010000ULL,0xda00011000ULL,0xda00011020ULL,
          0xda00011040ULL,0xda00011060ULL)==STATUS_INVALID_PARAMETER);
      s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS);
    }
    {
      struct a04_windows_render_map srmap[]={{0xda00010000ULL,0,5,0,0},
        {0xda00015000ULL,5,1,0,A04_GPU_MMU_READ_ONLY},
        {0xe000001000ULL,6,1,1,A04_GPU_MMU_READ_ONLY}};
      uint8_t *out;s.processor_released=TRUE;
      assert(a04_windows_render_memory_create(&m,&s,7,srmap,3)==STATUS_SUCCESS);
      out=(uint8_t *)m.cpu+A04_RENDER_PAYLOAD_OFFSET+5u*4096u;
      assert(a04_windows_render_stage_context_sr(&m,0xda00010000ULL,0xe000001000ULL,0xe000001400ULL,0,0,
        0xda00015000ULL,0xda00015020ULL,0xda00015080ULL,0xda000150c0ULL)==STATUS_INVALID_PARAMETER);
      for(unsigned i=0;i<208;++i) assert(!out[i]);
      ((uint8_t *)m.cpu)[A04_RENDER_PAYLOAD_OFFSET+16431]=1;
      assert(a04_windows_render_stage_context_sr(&m,0xda00010000ULL,0xe000001000ULL,0xe000001400ULL,0,0,
        0xda00015000ULL,0xda00015040ULL,0xda00015080ULL,0xda000150c0ULL)==STATUS_INVALID_DEVICE_STATE);
      assert(!m.writable_count && !m.staged_count);((uint8_t *)m.cpu)[A04_RENDER_PAYLOAD_OFFSET+16431]=0;
      assert(a04_windows_render_stage_context_sr(&m,0xda00010000ULL,0xe000001000ULL,0xe000001400ULL,0,0,
        0xda00015000ULL,0xda00015040ULL,0xda00015080ULL,0xda000150c0ULL)==STATUS_SUCCESS);
      assert(m.writable_count==1);
      assert(out[8]==2 && out[24]==2 && out[26]==0 && out[27]==0x80);
      assert(out[64]==1 && out[66]==2 && out[67]==0xf0 && out[76]==0);
      assert(a04_windows_render_stage_context_sr(&m,0xda00010000ULL,0xe000001000ULL,0xe000001400ULL,0,0,
        0xda00015000ULL,0xda00015040ULL,0xda00015080ULL,0xda000150c0ULL)==STATUS_INVALID_PARAMETER);
      { uint8_t saved[208];memcpy(saved,out,sizeof(saved));
        assert(a04_windows_render_stage_context_pt(&m,0xda00010000ULL,0xda00015030ULL,0xda00015100ULL,
          0xda00015120ULL,0xda00015140ULL)==STATUS_INVALID_PARAMETER);
        assert(!memcmp(saved,out,sizeof(saved)) && m.staged_count==4); }
      assert(a04_windows_render_stage_fragment_pds(&m,0xda00015040ULL,0xe000001000ULL,0)==STATUS_INVALID_PARAMETER);
      s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS);
    }
    {
      struct a04_windows_render_map bm[]={{0xda00010000ULL,0,5,0,0},
        {0xda00015000ULL,5,1,0,A04_GPU_MMU_READ_ONLY},
        {0xe000001000ULL,6,1,1,A04_GPU_MMU_READ_ONLY}};
      uint8_t *shader,*pds;s.processor_released=TRUE;
      assert(a04_windows_render_memory_create(&m,&s,7,bm,3)==STATUS_SUCCESS);
      shader=(uint8_t *)m.cpu+A04_RENDER_PAYLOAD_OFFSET+6u*4096u;
      pds=(uint8_t *)m.cpu+A04_RENDER_PAYLOAD_OFFSET+5u*4096u;
      assert(a04_windows_render_stage_bounded_context_sr(&m,0xda00010000ULL,0xe000001000ULL,0xe000001400ULL,
        0xda00015000ULL,0xda00015040ULL,0xda00015080ULL,0xda000150c0ULL)==STATUS_INVALID_DEVICE_STATE);
      shader[767]=1;
      assert(a04_windows_render_stage_bounded_context_usc(&m,0xe000001000ULL,0xe000001400ULL)==STATUS_INVALID_DEVICE_STATE);
      assert(!m.staged_count);shader[767]=0;
      assert(a04_windows_render_stage_bounded_context_usc(&m,0xe000001000ULL,0xe000001400ULL)==STATUS_SUCCESS);
      assert(m.staged_count==2 && !shader[767] && !shader[1024+639]);
      shader[0]^=1;
      assert(a04_windows_render_stage_bounded_context_sr(&m,0xda00010000ULL,0xe000001000ULL,0xe000001400ULL,
        0xda00015000ULL,0xda00015040ULL,0xda00015080ULL,0xda000150c0ULL)==STATUS_INVALID_DEVICE_STATE);
      shader[0]^=1;
      assert(a04_windows_render_stage_bounded_context_sr(&m,0xda00010000ULL,0xe000001000ULL,0xe000001400ULL,
        0xda00015000ULL,0xda00015040ULL,0xda00015080ULL,0xda000150c0ULL)==STATUS_SUCCESS);
      assert(m.staged_count==6 && m.writable_count==1 && pds[32]==0 && pds[36]==0x18 && pds[160]==0 && pds[164]==0x18);
      assert(a04_windows_render_check_bounded_states(&m)==STATUS_INVALID_PARAMETER);
      { uint8_t *state=(uint8_t *)m.cpu+A04_RENDER_PAYLOAD_OFFSET;m.completion_observed=TRUE;
        state[0]=0xff;state[1]=0xff;state[2]=0xff;state[3]=0xff;
        assert(a04_windows_render_check_bounded_states(&m)==STATUS_DATA_ERROR && m.bounded_states_checked && !m.bounded_states[0].valid);
        state[0]=3;state[1]=state[2]=state[3]=0;
        assert(a04_windows_render_check_bounded_states(&m)==STATUS_SUCCESS && m.bounded_states[0].valid && m.bounded_states[0].blocks==3); }
      s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS);
    }
    {
      struct a04_windows_render_map cm[]={{0xda00010000ULL,0,10,0,0},
        {0xda0001a000ULL,10,1,0,A04_GPU_MMU_READ_ONLY},{0xda0001b000ULL,11,1,0,0},
        {0xe000001000ULL,12,1,1,A04_GPU_MMU_READ_ONLY},{0x8000000000ULL,13,1,2,A04_GPU_MMU_SLC_BYPASS}};
      struct a04_render_context_static_input in={0};struct rogue_fwif_geom_registers_caswitch cs={0},saved;
      uint8_t *states;unsigned i;s.processor_released=TRUE;
      assert(a04_windows_render_memory_create(&m,&s,14,cm,5)==STATUS_SUCCESS);
      in.vdm_state=0x8000000000ULL;in.geom_state=0x8000000100ULL;
      for(i=0;i<2;++i) {
        uint64_t pds=0xda0001a000ULL+i*0x80u,sr=0xda0001a100ULL+i*0x100u;
        uint64_t shader=0xe000001000ULL+i*0x800u;struct a04_render_context_static_slot *q=&in.slot[i];
        q->pt_store_data=pds;q->pt_store_code=pds+32;q->pt_resume_data=pds+64;q->pt_resume_code=pds+96;
        q->sr_store_data=sr;q->sr_store_code=sr+64;q->sr_resume_data=sr+128;q->sr_resume_code=sr+192;
        assert(a04_windows_render_stage_context_pt(&m,0xda0001b000ULL+i*128u,q->pt_store_data,q->pt_store_code,
          q->pt_resume_data,q->pt_resume_code)==STATUS_SUCCESS);
        assert(a04_windows_render_stage_bounded_context_usc(&m,shader,shader+0x400u)==STATUS_SUCCESS);
        assert(a04_windows_render_stage_bounded_context_sr(&m,0xda00010000ULL+i*0x5000u,shader,shader+0x400u,
          q->sr_store_data,q->sr_store_code,q->sr_resume_data,q->sr_resume_code)==STATUS_SUCCESS);
      }
      saved=cs;in.slot[1].pt_store_code+=16;
      assert(a04_windows_render_make_bounded_static(&m,&in,&cs)==STATUS_INVALID_PARAMETER && !memcmp(&cs,&saved,sizeof(cs)));
      in.slot[1].pt_store_code-=16;states=(uint8_t *)m.cpu+A04_RENDER_PAYLOAD_OFFSET+10u*4096u;
      states[0]^=1;
      assert(a04_windows_render_make_bounded_static(&m,&in,&cs)==STATUS_INVALID_DEVICE_STATE && !memcmp(&cs,&saved,sizeof(cs)));
      states[0]^=1;states=(uint8_t *)m.cpu+A04_RENDER_PAYLOAD_OFFSET+13u*4096u;states[0x100]=1;
      assert(a04_windows_render_make_bounded_static(&m,&in,&cs)==STATUS_INVALID_DEVICE_STATE && !memcmp(&cs,&saved,sizeof(cs)));
      states[0x100]=0;
      assert(a04_windows_render_make_bounded_static(&m,&in,&cs)==STATUS_SUCCESS && m.writable_count==6);
      assert(cs.geom_reg_vdm_context_state_base_addr==in.vdm_state && cs.geom_reg_ta_context_state_base_addr==in.geom_state);
      assert(a04_windows_render_make_bounded_static(&m,&in,&cs)==STATUS_INVALID_PARAMETER);
      assert(a04_windows_render_memory_bind_vm(&m)==STATUS_SUCCESS);
      assert(a04_windows_render_prepare_context(&m,7,in.vdm_state,4,&cs)==STATUS_INVALID_PARAMETER);
      cs.geom_state[0].geom_reg_vdm_context_store_task0^=1;
      assert(a04_windows_render_prepare_context(&m,7,0x8000000400ULL,4,&cs)==STATUS_INVALID_PARAMETER);
      cs.geom_state[0].geom_reg_vdm_context_store_task0^=1;
      assert(a04_windows_render_prepare_context(&m,7,0x8000000400ULL,4,&cs)==STATUS_SUCCESS);
      { struct rogue_fwif_fwrendercontext *c=m.render.context.cpu;
        c->static_render_context_state.ctxswitch_regs[0].geom_reg_ta_context_state_base_addr^=16;
        assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && !m.published);
        c->static_render_context_state.ctxswitch_regs[0].geom_reg_ta_context_state_base_addr^=16;
        states=(uint8_t *)m.cpu+A04_RENDER_PAYLOAD_OFFSET+10u*4096u;states[0]^=1;
        assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && !m.published);
        states[0]^=1;
        assert(a04_windows_render_publish_context(&m)==STATUS_SUCCESS && m.published); }
      s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS);
    }
    {
      struct a04_windows_render_map vmaps[]={{0xda00010000ULL,0,1,0,A04_GPU_MMU_READ_ONLY},
        {0xda00011000ULL,1,2,0,0},{0xda00013000ULL,3,12,0,0},
        {0xe000005000ULL,15,1,1,A04_GPU_MMU_READ_ONLY},
        {0x8000000000ULL,16,4,2,0}};
      struct a04_render_pds_input p={0};uint8_t *out;s.processor_released=TRUE;
      assert(a04_windows_render_memory_create(&m,&s,20,vmaps,5)==STATUS_SUCCESS);
      out=(uint8_t *)m.cpu+A04_RENDER_PAYLOAD_OFFSET;
      p.shader_gpu=0xe000005040ULL;p.spill_info_gpu=0xda00010040ULL;p.scratch_info_gpu=0xda00010050ULL;
      p.spill_buffer_gpu=0xda00011000ULL;p.scratch_buffer_gpu=0xda00013000ULL;
      p.doutu_control=UINT64_C(1)<<35;p.vertex_literal=0xabc;p.descriptor_literal0=4;p.descriptor_literal1=5;
      p.shader_gpu=0xe000005ff0ULL;
      assert(a04_windows_render_stage_vertex_pds(&m,&p,0xda00010000ULL,0xda00010020ULL)==STATUS_INVALID_PARAMETER);
      p.shader_gpu=0xe000005040ULL;out[0x27]=1;
      assert(a04_windows_render_stage_vertex_pds(&m,&p,0xda00010000ULL,0xda00010020ULL)==STATUS_INVALID_DEVICE_STATE);
      assert(!m.staged_count && !out[0]);out[0x27]=0;
      assert(a04_windows_render_stage_vertex_pds(&m,&p,0xda00010000ULL,0xda00010008ULL)==STATUS_INVALID_PARAMETER);
      for(unsigned i=0;i<96;++i) assert(!out[i]);
      assert(a04_windows_render_stage_vertex_pds(&m,&p,0xda00010000ULL,0xda00010020ULL)==STATUS_SUCCESS);
      assert(out[0]==0x40 && out[1]==0x50 && out[4]==8 && out[8]==0xbc && out[9]==0x0a);
      assert(out[0x20]==0x40 && out[0x24]==0xda && out[0x30]==4 && out[0x34]==5);
      assert(out[0x40]==0 && out[0x41]==0x10 && out[0x48]==4 && out[0x58]==24);
      for(unsigned i=12;i<16;++i) assert(!out[i]);
      for(unsigned i=0x38;i<0x40;++i) assert(!out[i]);
      assert(a04_windows_render_stage_vertex_pds(&m,&p,0xda00010000ULL,0xda00010020ULL)==STATUS_INVALID_PARAMETER);
      assert(m.staged_count==4);
      assert(a04_windows_render_stage_vertex_pds_code(&m,0xda00010080ULL,0xda00010080ULL)==STATUS_INVALID_PARAMETER);
      out[0x90]=1;
      assert(a04_windows_render_stage_vertex_pds_code(&m,0xda00010080ULL,0xda00010090ULL)==STATUS_INVALID_DEVICE_STATE);
      assert(!out[0x80] && m.staged_count==4);out[0x90]=0;
      assert(a04_windows_render_stage_vertex_pds_code(&m,0xda00010080ULL,0xda00010090ULL)==STATUS_SUCCESS);
      assert(out[0x80]==1 && out[0x81]==0x40 && out[0x83]==0xf0 && out[0x87]==0xfc && out[0x8b]==0xd3);
      assert(out[0x93]==0xf0 && out[0x94]==0 && out[0x95]==1 && out[0x97]==0xf4);
      for(unsigned i=0x98;i<0xa0;++i) assert(!out[i]);
      assert(a04_windows_render_stage_vertex_pds_code(&m,0xda00010080ULL,0xda00010090ULL)==STATUS_INVALID_PARAMETER);
      p.scratch_info_gpu=0;p.scratch_buffer_gpu=0;p.descriptor_literal1=0;p.spill_info_gpu=0xda000100c0ULL;
      p.vertex_literal=0x80000000u;p.descriptor_literal0=0x90000003u;p.doutu_control=UINT64_C(0x800000000);
      assert(a04_windows_render_stage_scratchless_pds(&m,&p,0xda000100a0ULL,0xda000100b0ULL)==STATUS_SUCCESS);
      assert(out[0xa0]==0x40 && out[0xa1]==0x50 && out[0xb0]==0xc0 && out[0xb4]==0xda && out[0xb8]==3 && out[0xbb]==0x90);
      assert(out[0xc1]==0x10 && out[0xc8]==4);
      for(unsigned i=0xbc;i<0xc0;++i) assert(!out[i]);
      assert(a04_windows_render_stage_scratchless_pds_code(&m,0xda000100d0ULL,0xda000100e0ULL)==STATUS_SUCCESS);
      assert(out[0xd0]==1 && out[0xd3]==0xf0 && out[0xe0]==0 && out[0xe2]==2 && out[0xe3]==0xf4);
      for(unsigned i=0xe4;i<0xf0;++i) assert(!out[i]);
      assert(m.staged_count==11);
      { struct a04_render_fixed_vdm_input v={0xda000100b0ULL,0xda000100e0ULL,
          0xda000100a0ULL,0xda000100d0ULL,0xda000100f0ULL,0xda00010160ULL};
        assert(a04_windows_render_stage_fixed_vdm(&m,&v,0xda000100f0ULL)==STATUS_INVALID_PARAMETER);
        out[0x180]=1;
        assert(a04_windows_render_stage_fixed_vdm(&m,&v,0xda00010180ULL)==STATUS_INVALID_DEVICE_STATE);
        assert(m.staged_count==11);out[0x180]=0;
        assert(a04_windows_render_stage_fixed_vdm(&m,&v,0xda00010180ULL)==STATUS_SUCCESS);
        assert(out[0x180]==1 && out[0x182]==1 && out[0x183]==0x20);
        assert(out[0x18c]==0xda && out[0x18d]==25 && out[0x194]==0x80 && out[0x197]==0x4c);
        assert(out[0x1a8]==0 && out[0x1a9]==6 && out[0x1ab]==0x68 && out[0x1ac]==3);
        assert(out[0x1bb]==0xc0 && m.staged_count==12);
        assert(a04_windows_render_stage_fixed_vdm(&m,&v,0xda00010180ULL)==STATUS_INVALID_PARAMETER); }
      { struct a04_render_fragment_uniform_input f={0xda00010220ULL,0xda00011000ULL,0xda00010230ULL};
        assert(a04_windows_render_stage_fragment_uniform(&m,&f,0xda00010200ULL,0xda00010200ULL)==STATUS_INVALID_PARAMETER);
        out[0x240]=1;
        assert(a04_windows_render_stage_fragment_uniform(&m,&f,0xda00010200ULL,0xda00010240ULL)==STATUS_INVALID_DEVICE_STATE);
        assert(!out[0x200] && m.staged_count==12);out[0x240]=0;
        assert(a04_windows_render_stage_fragment_uniform(&m,&f,0xda00010200ULL,0xda00010240ULL)==STATUS_SUCCESS);
        assert(out[0x200]==0x20 && out[0x201]==2 && out[0x204]==0xda);
        assert(out[0x208]==0x30 && out[0x209]==2 && out[0x210]==3 && out[0x211]==0x80 && out[0x213]==0x10);
        assert(out[0x214]==1 && out[0x217]==0x90 && out[0x220]==0 && out[0x221]==0x10 && out[0x228]==4);
        assert(out[0x230]==0xfe && out[0x231]==0xff && out[0x232]==0xff && out[0x233]==1);
        assert(out[0x242]==4 && out[0x243]==0xf0 && out[0x245]==1 && out[0x246]==5 && out[0x247]==0xf4);
        for(unsigned i=0x218;i<0x220;++i) assert(!out[i]);
        assert(m.staged_count==16); }
      { struct a04_render_fixed_ppp_input pp={0xda00010300ULL,0xda00010240ULL,0xda00010200ULL};
        struct a04_render_fixed_vdm_input v={0xda000100b0ULL,0xda000100e0ULL,
          0xda000100a0ULL,0xda000100d0ULL,0xda00010400ULL,0xda00010470ULL};
        assert(a04_windows_render_stage_fragment_pds(&m,pp.fragment_pds,0xe000005040ULL,0)==STATUS_SUCCESS);
        assert(a04_windows_render_stage_fixed_ppp(&m,&pp,0xda00010400ULL,0xda00010400ULL)==STATUS_INVALID_PARAMETER);
        out[0x470]=1;
        assert(a04_windows_render_stage_fixed_ppp(&m,&pp,0xda00010400ULL,0xda00010470ULL)==STATUS_INVALID_DEVICE_STATE);
        assert(!out[0x400] && m.staged_count==17);out[0x470]=0;
        assert(a04_windows_render_stage_fixed_ppp(&m,&pp,0xda00010400ULL,0xda00010470ULL)==STATUS_SUCCESS);
        assert(out[0x400]==0x67 && out[0x401]==0x0e && out[0x402]==0x7f && out[0x414]==0 && out[0x415]==3);
        assert(out[0x474]==1 && out[0x476]==4 && out[0x473]==2);
        assert(a04_windows_render_stage_fixed_vdm(&m,&v,0xda00010500ULL)==STATUS_SUCCESS);
        assert(m.staged_count==20); }
      assert(a04_windows_render_stage_fixed_scissor(&m,0xda00010000ULL)==STATUS_INVALID_PARAMETER);
      out[0x600]=1;assert(a04_windows_render_stage_fixed_scissor(&m,0xda00010600ULL)==STATUS_INVALID_DEVICE_STATE);
      out[0x600]=0;assert(a04_windows_render_stage_fixed_scissor(&m,0xda00010600ULL)==STATUS_SUCCESS);
      assert(out[0x600]==49 && out[0x604]==64 && m.staged_count==21);
      assert(a04_windows_render_stage_pixel_event(&m,0xda00010610ULL,0xe000005ff0ULL)==STATUS_INVALID_PARAMETER);
      out[0x61f]=1;assert(a04_windows_render_stage_pixel_event(&m,0xda00010610ULL,0xe000005100ULL)==STATUS_INVALID_DEVICE_STATE);
      assert(m.staged_count==21 && !out[0x610]);out[0x61f]=0;
      assert(a04_windows_render_stage_pixel_event(&m,0xda00010610ULL,0xe000005100ULL)==STATUS_SUCCESS);
      assert(out[0x610]==0 && out[0x611]==0x51 && out[0x614]==8 && m.staged_count==22);
      for(unsigned i=0x618;i<0x620;++i) assert(!out[i]);
      { uint8_t *shader=out+15u*4096u+0x200u;
        assert(a04_windows_render_stage_fixed_eot(&m,0xe000005200ULL,0xda00010000ULL)==STATUS_INVALID_PARAMETER);
        shader[40]=1;
        assert(a04_windows_render_stage_fixed_eot(&m,0xe000005200ULL,0x8000000000ULL)==STATUS_INVALID_DEVICE_STATE);
        assert(m.staged_count==22 && !shader[0]);shader[40]=0;
        assert(a04_windows_render_stage_fixed_eot(&m,0xe000005200ULL,0x8000000000ULL)==STATUS_SUCCESS);
        assert(shader[0]==0x86 && shader[1]==0x92 && shader[4]==0x20 && shader[16]==0 && m.staged_count==24);
        assert(a04_windows_render_stage_fixed_eot(&m,0xe000005200ULL,0x8000000000ULL)==STATUS_INVALID_PARAMETER);
        for(unsigned i=40;i<128;++i) assert(!shader[i]); }
      assert(a04_windows_render_stage_fixed_background(&m,0xda00010630ULL,0xda00010630ULL,0xda00010680ULL)==STATUS_INVALID_PARAMETER);
      out[0x640]=1;
      assert(a04_windows_render_stage_fixed_background(&m,0xda00010630ULL,0xda00010640ULL,0xda00010680ULL)==STATUS_INVALID_DEVICE_STATE);
      assert(!out[0x630] && m.staged_count==24);out[0x640]=0;
      assert(a04_windows_render_stage_fixed_background(&m,0xda00010630ULL,0xda00010640ULL,0xda00010680ULL)==STATUS_SUCCESS);
      assert(out[0x630]==0x22 && out[0x633]==0xff && out[0x640]==0x30 && out[0x641]==6 &&
             out[0x644]==0xda && out[0x648]==1 && out[0x64b]==0x90 && out[0x683]==0xf4 && out[0x687]==0xd3 && m.staged_count==27);
      out[0x67f]=1;
      assert(a04_windows_render_stage_background_shader(&m,0xe000005300ULL,0xda00010650ULL)==STATUS_INVALID_DEVICE_STATE);
      assert(m.staged_count==27);out[0x67f]=0;
      assert(a04_windows_render_stage_background_shader(&m,0xe000005300ULL,0xda00010650ULL)==STATUS_SUCCESS);
      assert(out[15u*4096u+0x300]==0x38 && out[0x650]==0 && out[0x651]==0x53 &&
             out[0x654]==0 && out[0x670]==2 && out[0x673]==0xf4 && m.staged_count==29);
      assert(a04_windows_render_stage_background_shader(&m,0xe000005300ULL,0xda00010650ULL)==STATUS_INVALID_PARAMETER);
      { uint8_t *sh=out+15u*4096u;
        assert(a04_windows_render_stage_triangle_usc(&m,0xe000005700ULL,0xe0000057c0ULL)==STATUS_INVALID_PARAMETER);
        sh[0x9bf]=1;
        assert(a04_windows_render_stage_triangle_usc(&m,0xe000005700ULL,0xe000005800ULL)==STATUS_INVALID_DEVICE_STATE);
        assert(m.staged_count==29 && !m.triangle_usc_staged);sh[0x9bf]=0;
        assert(a04_windows_render_stage_triangle_usc(&m,0xe000005700ULL,0xe000005800ULL)==STATUS_SUCCESS);
        assert(sh[0x700]==0x99 && sh[0x800]==0x44 && m.staged_count==31 && m.triangle_usc_staged);
        for(unsigned i=176;i<256;++i) assert(!sh[0x700+i]);
        for(unsigned i=360;i<448;++i) assert(!sh[0x800+i]);
        sh[0x700]^=1;
        assert(a04_windows_render_stage_triangle_vertex_pds(&m,0xda00010690ULL,0xda00011000ULL,0xda000106a0ULL,0xda000106b0ULL)==STATUS_INVALID_DEVICE_STATE);
        sh[0x700]^=1;
        out[4096]=1;
        assert(a04_windows_render_stage_triangle_vertex_pds(&m,0xda00010690ULL,0xda00011000ULL,0xda000106a0ULL,0xda000106b0ULL)==STATUS_INVALID_DEVICE_STATE);
        out[4096]=0;
        assert(a04_windows_render_stage_triangle_vertex_pds(&m,0xda00010690ULL,0xda00011000ULL,0xda000106a0ULL,0xda000106b0ULL)==STATUS_SUCCESS);
        assert(a04_windows_render_stage_triangle_fragment_pds(&m,0xda000106c0ULL)==STATUS_SUCCESS);
        assert(out[0x6a1]==0x57 && out[0x6a4]==8 && out[0x6c1]==0x58 && out[0x6c4]==0x10 && m.staged_count==35);
        assert(a04_windows_render_stage_triangle_uniform(&m,0xda00010720ULL,0xda00011000ULL,0xda00010730ULL,0xda00010700ULL,0xda00010740ULL)==STATUS_INVALID_DEVICE_STATE);
        assert(a04_windows_render_stage_triangle_uniform(&m,0xda00010720ULL,0xda00013000ULL,0xda00010730ULL,0xda00010700ULL,0xda00010740ULL)==STATUS_SUCCESS);
        assert(a04_windows_render_stage_triangle_vertex_code(&m,0xda00010750ULL,0xda00010760ULL)==STATUS_SUCCESS);
        assert(m.triangle_vertex_pds_staged && m.triangle_vertex_code_staged && m.triangle_fragment_pds_staged && m.triangle_uniform_staged && m.writable_count==2 && m.staged_count==41);
        assert(a04_windows_render_memory_bind_vm(&m)==STATUS_SUCCESS);
        assert(a04_windows_render_prepare_context(&m,9,0xda00016000ULL,4,&static_state)==STATUS_SUCCESS);
        out[0x6a0]^=1;
        assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && !m.published);
        out[0x6a0]^=1;out[4096]=1;
        assert(a04_windows_render_publish_context(&m)==STATUS_INVALID_DEVICE_STATE && !m.published);
        out[4096]=0;
        assert(a04_windows_render_publish_context(&m)==STATUS_SUCCESS && m.published); }
      s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS);
    }
    { struct a04_windows_render_map sm[]={{0xda00000000ULL,0,1,0,A04_GPU_MMU_READ_ONLY},{0xe000000000ULL,1,1,1,A04_GPU_MMU_READ_ONLY}};
      uint8_t *p;s.processor_released=TRUE;assert(a04_windows_render_memory_create(&m,&s,2,sm,2)==STATUS_SUCCESS);
      p=(uint8_t *)m.cpu+A04_RENDER_PAYLOAD_OFFSET;p[128]=1;
      assert(a04_windows_render_stage_static_heap_programs(&m)==STATUS_INVALID_DEVICE_STATE && !m.staged_count);
      p[128]=0;assert(a04_windows_render_stage_static_heap_programs(&m)==STATUS_SUCCESS && m.staged_count==2);
      assert(p[32]==2 && p[35]==0xf4 && p[128]==2 && p[131]==0xf4 && p[4096]==0x44 && p[4096+7]==0xff);
      assert(a04_windows_render_stage_static_heap_programs(&m)==STATUS_INVALID_PARAMETER);
      s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS); }
    { struct a04_windows_render_diagnostic d={0};s.processor_released=TRUE;s.firmware_started=TRUE;s.queue_faulted=FALSE;
      cleanup_calls=0;kick_calls=0;diagnostic_mock=&d;
      assert(a04_windows_render_diagnostic_run(&d,&s)==STATUS_SUCCESS);
      assert(d.stage==32 && d.background_pixels==2989 && d.triangle_pixels==1107 && !d.other_pixels && !d.spatial_mismatch);
      assert(d.memory.cleanup_complete && d.memory.bounded_states_checked && cleanup_calls==5 && kick_calls==2);
      assert(a04_windows_render_diagnostic_run(&d,&s)==STATUS_INVALID_PARAMETER);
      diagnostic_mock=NULL;s.processor_released=FALSE;assert(a04_windows_render_memory_free(&d.memory,TRUE)==STATUS_SUCCESS); }
    { struct a04_windows_render_diagnostic d={0};s.processor_released=TRUE;cleanup_calls=0;kick_calls=0;kick_fail_at=1;diagnostic_mock=&d;
      assert(a04_windows_render_diagnostic_run(&d,&s)==STATUS_IO_TIMEOUT && d.stage==30 && d.memory.poisoned && !d.memory.cleanup_complete);
      assert(a04_windows_render_memory_free(&d.memory,TRUE)==STATUS_DEVICE_BUSY);
      diagnostic_mock=NULL;kick_fail_at=0;s.processor_released=FALSE;assert(a04_windows_render_memory_free(&d.memory,TRUE)==STATUS_SUCCESS); }
    { struct a04_windows_render_diagnostic d={0};s.processor_released=TRUE;cleanup_calls=0;kick_calls=0;cleanup_fail_at=3;diagnostic_mock=&d;
      assert(a04_windows_render_diagnostic_run(&d,&s)==STATUS_DEVICE_BUSY && d.stage==31 && d.memory.poisoned && !d.memory.cleanup_complete);
      assert(cleanup_calls==3);diagnostic_mock=NULL;cleanup_fail_at=0;s.processor_released=FALSE;assert(a04_windows_render_memory_free(&d.memory,TRUE)==STATUS_SUCCESS); }
    { struct a04_windows_render_diagnostic d={0};s.processor_released=TRUE;cleanup_calls=0;kick_calls=0;diagnostic_wrong_pixels=1;diagnostic_mock=&d;
      assert(a04_windows_render_diagnostic_run(&d,&s)==STATUS_DATA_ERROR && d.stage==32 && d.memory.cleanup_complete && d.other_pixels==4096);
      diagnostic_mock=NULL;diagnostic_wrong_pixels=0;s.processor_released=FALSE;assert(a04_windows_render_memory_free(&d.memory,TRUE)==STATUS_SUCCESS); }
    { struct a04_windows_render_diagnostic d={0};s.processor_released=TRUE;cleanup_calls=0;kick_calls=0;diagnostic_wrong_pixels=2;diagnostic_mock=&d;
      assert(a04_windows_render_diagnostic_run(&d,&s)==STATUS_DATA_ERROR && d.stage==32 && d.triangle_pixels==1107 && d.other_pixels==0 && d.spatial_mismatch>0);
      diagnostic_mock=NULL;diagnostic_wrong_pixels=0;s.processor_released=FALSE;assert(a04_windows_render_memory_free(&d.memory,TRUE)==STATUS_SUCCESS); }
    { struct a04_windows_render_diagnostic d={0};s.processor_released=TRUE;cleanup_calls=0;kick_calls=0;diagnostic_wrong_pixels=3;diagnostic_mock=&d;
      assert(a04_windows_render_diagnostic_run(&d,&s)==STATUS_DATA_ERROR && d.stage==32 && d.triangle_pixels==1107 && d.other_pixels==0 && d.spatial_mismatch>0);
      diagnostic_mock=NULL;diagnostic_wrong_pixels=0;s.processor_released=FALSE;assert(a04_windows_render_memory_free(&d.memory,TRUE)==STATUS_SUCCESS); }
    { struct a04_windows_render_diagnostic d={0};s.processor_released=TRUE;cleanup_calls=0;kick_calls=0;diagnostic_wrong_pixels=4;diagnostic_mock=&d;
      assert(a04_windows_render_diagnostic_run(&d,&s)==STATUS_DATA_ERROR && d.stage==32 && d.zero_pixels==4096 && d.other_pixels==4096 && !d.triangle_pixels);
      assert(d.zero_rows[0][0]==0xffffffffu && d.zero_rows[63][1]==0xffffffffu);
      diagnostic_mock=NULL;diagnostic_wrong_pixels=0;s.processor_released=FALSE;assert(a04_windows_render_memory_free(&d.memory,TRUE)==STATUS_SUCCESS); }
    free(s.memory.heap);
    return 0;
}
