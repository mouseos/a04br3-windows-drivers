/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "windows_render_memory.h"
#include "gpu_mmu.h"
static unsigned allocated,freed,cleans,invalidations,fail_sync,fail_inv,expect_staged;
static void *alloc_buf(PDMA_ADAPTER a,ULONG n,PHYSICAL_ADDRESS *pa,BOOLEAN cache)
{ (void)a;(void)cache;pa->QuadPart=0x10000000u;++allocated;return aligned_alloc(4096,n); }
static void free_buf(PDMA_ADAPTER a,ULONG n,PHYSICAL_ADDRESS pa,void *p,BOOLEAN cache)
{ (void)a;(void)n;(void)pa;(void)cache;++freed;free(p); }
unsigned KeGetCurrentIrql(void) { return PASSIVE_LEVEL; }
NTSTATUS a04_gpu_dma_sync(void *p,ULONG n,BOOLEAN rd)
{ assert(p && n && !rd);if(expect_staged) { const unsigned char *b=(const unsigned char *)p+A04_RENDER_PAYLOAD_OFFSET+4u*4096u;
  assert(b[0]==0x40 && b[1]==0x50 && b[4]==8 && b[32]==2 && b[35]==0xf4); }
  ++cleans;return fail_sync ? STATUS_DEVICE_CONFIGURATION_ERROR : STATUS_SUCCESS; }
NTSTATUS a04_gpu_firmware_invalidate_mmu(struct a04_gpu_firmware_state *s)
{ assert(s);++invalidations;return fail_inv ? STATUS_IO_TIMEOUT : STATUS_SUCCESS; }
int main(void)
{
    DMA_OPERATIONS ops={alloc_buf,free_buf};struct fake_adapter adapter={&ops};
    struct a04_gpu_firmware_state s={0};struct a04_windows_render_memory m={0};
    struct a04_windows_render_map maps[]={{0x80000b6000ULL,0,4,0,0},{0xda00002000ULL,4,1,1,A04_GPU_MMU_READ_ONLY},
        {0xe000005000ULL,5,1,2,A04_GPU_MMU_READ_ONLY}};
    s.memory.adapter=&adapter;s.memory.heap=(void *)1;s.memory.heap_logical.QuadPart=0x80000000u;
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
    expect_staged=1;assert(a04_windows_render_memory_publish(&m)==STATUS_SUCCESS && m.published && !m.poisoned);expect_staged=0;
    assert(a04_windows_render_stage_fragment_pds(&m,0xda00002000ULL,0xe000005040ULL,0)==STATUS_INVALID_PARAMETER);
    assert(cleans==1 && invalidations==1);
    assert(a04_windows_render_memory_publish(&m)==STATUS_INVALID_PARAMETER);
    s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,FALSE)==STATUS_DEVICE_BUSY && !freed);
    assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS && freed==1);
    s.processor_released=TRUE;assert(a04_windows_render_memory_create(&m,&s,6,maps,3)==STATUS_SUCCESS);
    fail_inv=1;assert(a04_windows_render_memory_publish(&m)==STATUS_IO_TIMEOUT && m.poisoned && !m.published);
    assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_DEVICE_BUSY && freed==1);
    s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS && freed==2);
    s.processor_released=TRUE;maps[1].payload_page=0;
    assert(a04_windows_render_memory_create(&m,&s,5,maps,2)==STATUS_INVALID_PARAMETER && allocated==2);
    maps[1].payload_page=4;maps[1].gpu_va=maps[0].gpu_va;
    assert(a04_windows_render_memory_create(&m,&s,5,maps,2)==STATUS_INVALID_PARAMETER && allocated==2);
    maps[0].pages=513;
    assert(a04_windows_render_memory_create(&m,&s,513,maps,1)==STATUS_DEVICE_CONFIGURATION_ERROR);
    assert(m.cpu && !m.ready);
    assert(a04_windows_render_memory_publish(&m)==STATUS_INVALID_PARAMETER);
    s.processor_released=FALSE;assert(a04_windows_render_memory_free(&m,TRUE)==STATUS_SUCCESS);
    return 0;
}
