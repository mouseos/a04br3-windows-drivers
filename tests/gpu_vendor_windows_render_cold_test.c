/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "windows_render_cold.h"
static unsigned allocs,frees,pools,fail_at;
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
void a04_gpu_dma_barrier(void){}
void *ExAllocatePoolWithTag(int type,size_t size,unsigned tag)
{ (void)type;(void)tag;void *p=aligned_alloc(4096,(size+4095)&~(size_t)4095);if(p)pools++;return p; }
void ExFreePoolWithTag(void *p,unsigned tag){(void)tag;pools--;free(p);}
static void *allocate(PDMA_ADAPTER a,PHYSICAL_ADDRESS *max,ULONG n,PHYSICAL_ADDRESS *pa,BOOLEAN cache,ULONG node)
{
    (void)a;assert(max->QuadPart==0xffffffffLL && !cache && !node);
    allocs++;if(allocs==fail_at)return NULL;
    pa->QuadPart=0x80000000LL+(int64_t)allocs*0x1000000LL;return aligned_alloc(4096,n);
}
static void release(PDMA_ADAPTER a,ULONG n,PHYSICAL_ADDRESS pa,void *p,BOOLEAN cache)
{(void)a;(void)n;(void)pa;assert(!cache);frees++;free(p);}
int main(void)
{
    DMA_OPERATIONS ops={.FreeCommonBuffer=release,.Size=sizeof(ops),.AllocateCommonBufferEx=allocate};
    struct fake_adapter adapter={&ops};struct a04_vendor_m1_11_memory f={0};
    struct a04_vendor_render_cold r={0};struct a04_vendor_surface surface={0};uint32_t *table;unsigned i;
    const unsigned sizes[5]={32768,102400,20480,262144,65536};
    for(i=0;i<5;i++) {
        f.buffers[i].cpu=aligned_alloc(4096,sizes[i]);assert(f.buffers[i].cpu);
        f.buffers[i].bytes=sizes[i];f.buffers[i].dma=0xd0000000u+i*0x100000u;
        memset(f.buffers[i].cpu,0,sizes[i]);
    }
    f.adapter=&adapter;f.staged=TRUE;f.uncached=TRUE;table=f.buffers[0].cpu;table[0]=0x1234;
    fail_at=3;assert(a04_vendor_render_prepare_cold(&r,&f,&adapter,FALSE,0)==STATUS_INSUFFICIENT_RESOURCES);
    assert(frees==2 && !pools && !r.firmware && table[0]==0x1234 && !table[128]);
    fail_at=0;assert(a04_vendor_render_prepare_cold(&r,&f,&adapter,FALSE,0)==STATUS_SUCCESS);
    assert(r.mapped && table[0]==0x1234 && table[128] && r.layout.used<32768);
    assert(((uint32_t *)r.buffer[1].cpu)[A04_VENDOR_RENDER_SCRATCH_VA>>30]);
    f.hardware_owned=TRUE;assert(a04_vendor_render_release_cold(&r)==STATUS_DEVICE_BUSY && table[128]);
    f.hardware_owned=FALSE;assert(a04_vendor_render_release_cold(&r)==STATUS_SUCCESS && !table[128]);
    assert(frees==12 && !pools && !r.firmware);
    assert(a04_vendor_render_prepare_cold(&r,&f,&adapter,TRUE,0)==STATUS_SUCCESS);
    assert(r.clear_ready && ((unsigned char *)r.buffer[4].cpu)[0]==0xa5);
    assert(((uint32_t *)r.buffer[1].cpu)[UINT64_C(0xa800000000)>>30]);
    assert(((uint32_t *)r.buffer[1].cpu)[UINT64_C(0xba00000000)>>30]);
    assert(!memcmp(r.buffer[7].cpu,r.clear.pds,sizeof(r.clear.pds)));
    assert(!memcmp(r.buffer[10].cpu,r.clear.usc,sizeof(r.clear.usc)));
    assert(((uint32_t *)r.buffer[1].cpu)[UINT64_C(0xf800000000)>>30]);
    assert(((uint32_t *)r.buffer[1].cpu)[UINT64_C(0xf900000000)>>30]);
    assert(a04_vendor_surface_prepare(&surface,&adapter,800,1280,3328,UINT64_C(0x8000000000),1040)==STATUS_SUCCESS);
    assert(a04_vendor_render_attach_screen(&r,&surface)==STATUS_SUCCESS);
    assert(r.screen_surface==&surface && r.clear_first==1248u*3328u+768u*4u && r.clear_row_bytes==3328u);
    assert(((uint32_t *)r.buffer[1].cpu)[512]==((uint32_t)((uint64_t)surface.pd.logical.QuadPart>>8)|1u));
    assert(((uint32_t *)surface.pixels.cpu)[0]==0xa5a5a5a5u &&
           ((uint32_t *)surface.pixels.cpu)[surface.bytes/4u-1]==0xa5a5a5a5u);
    assert(a04_vendor_render_release_cold(&r)==STATUS_SUCCESS && frees==28 && !pools);
    assert(a04_vendor_surface_release(&surface)==STATUS_SUCCESS && frees==34);
    for(i=0;i<5;i++)free(f.buffers[i].cpu);
    return 0;
}
