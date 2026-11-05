/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "windows_render_cold.h"
#include "windows_copy_scratch.h"
#include "gpu_mmu.h"
static unsigned allocs,frees,pools,fail_at,alias_next;
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
void a04_gpu_dma_barrier(void){}
void *ExAllocatePoolWithTag(int type,size_t size,unsigned tag)
{ (void)type;(void)tag;void *p=aligned_alloc(4096,(size+4095)&~(size_t)4095);if(p)pools++;return p; }
void ExFreePoolWithTag(void *p,unsigned tag){(void)tag;pools--;free(p);}
static void *allocate(PDMA_ADAPTER a,PHYSICAL_ADDRESS *max,ULONG n,PHYSICAL_ADDRESS *pa,BOOLEAN cache,ULONG node)
{
    (void)a;assert(max->QuadPart==0xffffffffLL && !cache && !node);
    allocs++;if(allocs==fail_at)return NULL;
    pa->QuadPart=alias_next ? 0xd0000000LL : 0x80000000LL+(int64_t)allocs*0x1000000LL;alias_next=0;return aligned_alloc(4096,n);
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
        f.allocation[i]=f.buffers[i].cpu;f.logical[i].QuadPart=f.buffers[i].dma;
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
    {
        struct a04_vendor_copy_scratch scratch={0};unsigned oldfrees=frees,oldallocs=allocs;
        uint64_t *pt=(uint64_t*)r.buffer[3].cpu,expected;uint32_t *pds=(uint32_t*)r.buffer[7].cpu;uint32_t clear_pds[16];
        memcpy(clear_pds,pds,sizeof(clear_pds));
        assert(f.render_owner==&r && !r.copy_owner && !pt[16] && !pt[17] && !pt[18]);
        { pid_t child=fork();int code;
          assert(child>=0);
          if(!child) {
              struct a04_vendor_copy_scratch bad={0};unsigned was=frees;
              alias_next=1;
              assert(a04_vendor_copy_scratch_prepare(&bad,&r,1)==STATUS_DEVICE_CONFIGURATION_ERROR);
              assert(bad.exposed && r.copy_owner==&bad && frees==was && !pt[16]);
              assert(a04_vendor_copy_scratch_release(&bad)==STATUS_DEVICE_BUSY);
              assert(a04_vendor_render_release_cold(&r)==STATUS_DEVICE_BUSY);
              _exit(0);
          }
          assert(waitpid(child,&code,0)==child && WIFEXITED(code) && WEXITSTATUS(code)==0);
        }
        assert(a04_vendor_copy_scratch_prepare(&scratch,&r,0)==STATUS_INVALID_DEVICE_STATE && allocs==oldallocs);
        fail_at=allocs+2;
        assert(a04_vendor_copy_scratch_prepare(&scratch,&r,1)==STATUS_INSUFFICIENT_RESOURCES && frees==oldfrees+1 && !r.copy_owner && !pt[16]);
        fail_at=0;oldfrees=frees;
        assert(a04_vendor_copy_scratch_prepare(&scratch,&r,1)==STATUS_SUCCESS && scratch.prepared && r.copy_owner==&scratch);
        assert(pt[16]==((uint64_t)scratch.source.logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u) &&
               pt[17]==((uint64_t)scratch.destination.logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u) &&
               pt[18]==((uint64_t)scratch.shared.logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u));
        assert(((uint32_t*)scratch.shared.cpu)[0]==scratch.state.shared[0]);
        assert(!memcmp(clear_pds,pds,sizeof(clear_pds)));
        assert(!memcmp((UCHAR*)pds+64,scratch.eot_pds,64));
        assert(!memcmp((UCHAR*)pds+192,scratch.kick.pds_fragment,64));
        assert(!memcmp((UCHAR*)pds+256,scratch.state.pds_data,16));
        assert(!memcmp((UCHAR*)pds+272,scratch.state.pds_code,16));
        assert(scratch.eot_pds[2]==(uint32_t)(A04_VENDOR_COPY_DEST_VA>>2));
        assert(scratch.eot_raw28==0x202u && scratch.eot_raw2c==96u && scratch.eot_raw30==64u && scratch.raw60==3u);
        assert(!((uint32_t*)r.buffer[10].cpu)[128/4]); /* no background USC supplied */
        pds[64/4]^=1;
        assert(a04_vendor_copy_scratch_release(&scratch)==STATUS_INVALID_DEVICE_STATE && frees==oldfrees);
        pds[64/4]^=1;
        assert(((uint32_t*)scratch.source.cpu)[0]==0x5a00c3a5u && ((uint32_t*)scratch.destination.cpu)[0]==0xa5a5a5a5u);
        assert(a04_vendor_render_release_cold(&r)==STATUS_DEVICE_BUSY);
        expected=pt[17];pt[17]^=4096;
        assert(a04_vendor_copy_scratch_release(&scratch)==STATUS_INVALID_DEVICE_STATE && frees==oldfrees && r.copy_owner==&scratch);
        pt[17]=expected;
        assert(a04_vendor_copy_scratch_release(&scratch)==STATUS_SUCCESS && frees==oldfrees+3 && !pt[16] && !pt[17] && !pt[18] && !r.copy_owner);
        assert(!memcmp(clear_pds,pds,sizeof(clear_pds)) && !pds[64/4] && !pds[192/4] && !pds[256/4] && !pds[272/4]);
    }
    assert(a04_vendor_surface_prepare(&surface,&adapter,800,1280,3328,UINT64_C(0x8000000000))==STATUS_SUCCESS);
    assert(a04_vendor_render_attach_screen(&r,&surface)==STATUS_SUCCESS);
    assert(r.screen_surface==&surface && r.clear_first==1248u*3328u+768u*4u && r.clear_row_bytes==3328u);
    assert(((uint32_t *)r.buffer[1].cpu)[512]==((uint32_t)((uint64_t)surface.pd.logical.QuadPart>>8)|1u));
    assert(((uint32_t *)surface.pixels.cpu)[0]==0xa5a5a5a5u &&
           ((uint32_t *)surface.pixels.cpu)[surface.bytes/4u-1]==0xa5a5a5a5u);
    assert(a04_vendor_render_release_cold(&r)==STATUS_SUCCESS && frees==32 && !pools);
    assert(a04_vendor_surface_release(&surface)==STATUS_SUCCESS && frees==38);
    {
        struct a04_vendor_copy_scratch retained={0};unsigned oldfrees;
        assert(a04_vendor_render_prepare_cold(&r,&f,&adapter,TRUE,0)==STATUS_SUCCESS);
        assert(a04_vendor_copy_scratch_prepare(&retained,&r,1)==STATUS_SUCCESS);
        f.hardware_owned=TRUE;assert(a04_vendor_copy_scratch_release(&retained)==STATUS_DEVICE_BUSY);f.hardware_owned=FALSE;
        assert(a04_vendor_copy_scratch_mark_exposed(&retained)==STATUS_SUCCESS);
        oldfrees=frees;
        assert(a04_vendor_copy_scratch_release(&retained)==STATUS_DEVICE_BUSY && frees==oldfrees);
        assert(a04_vendor_render_release_cold(&r)==STATUS_DEVICE_BUSY && f.render_owner==&r);
        /* Process exit models retained resources; no hardware was exposed. */
    }
    return 0;
}
