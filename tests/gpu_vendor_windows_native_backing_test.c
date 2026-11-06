/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "windows_native_cold_private.h"
#include "A04BR3RogueDeviceWords.h"
#include "native_terminate_stream.h"
#include "gpu_mmu.h"
static unsigned allocs,frees,pools,pool_calls,fail_at,fail_pool,alias_at;
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
void a04_gpu_dma_barrier(void){}
void *ExAllocatePoolWithTag(int type,size_t size,unsigned tag)
{
    void *p;(void)type;(void)tag;
    if(++pool_calls==fail_pool)return NULL;
    p=aligned_alloc(4096,(size+4095)&~(size_t)4095);if(p)pools++;return p;
}
void ExFreePoolWithTag(void *p,unsigned tag){(void)tag;assert(pools);pools--;free(p);}
static void *allocate(PDMA_ADAPTER a,PHYSICAL_ADDRESS *max,ULONG size,PHYSICAL_ADDRESS *pa,BOOLEAN cache,ULONG node)
{
    (void)a;assert(max->QuadPart==0xffffffffLL && !cache && !node);
    if(++allocs==fail_at)return NULL;
    pa->QuadPart=allocs==alias_at ? 0x10000000LL : 0x30000000LL+(int64_t)allocs*0x1000000LL;
    return aligned_alloc(4096,size);
}
static void release(PDMA_ADAPTER a,ULONG size,PHYSICAL_ADDRESS pa,void *cpu,BOOLEAN cache)
{(void)a;(void)size;(void)pa;assert(!cache);frees++;free(cpu);}
struct fixture {
    DMA_OPERATIONS ops;struct fake_adapter adapter;
    struct a04_vendor_m1_11_memory fw;struct a04_vendor_render_cold r;
    struct a04_vendor_surface screen;
    uint32_t original_pc[1024];
};
static void init(struct fixture *x)
{
    unsigned i;uint32_t *pc;uint64_t *pd,*pt;
    memset(x,0,sizeof(*x));allocs=frees=pools=pool_calls=fail_at=fail_pool=alias_at=0;
    x->ops=(DMA_OPERATIONS){.Size=sizeof(x->ops),.AllocateCommonBufferEx=allocate,.FreeCommonBuffer=release};
    x->adapter.DmaOperations=&x->ops;
    for(i=0;i<5;i++) {
        x->fw.buffers[i].cpu=aligned_alloc(4096,32768);assert(x->fw.buffers[i].cpu);
        x->fw.buffers[i].dma=0x20000000u+i*0x100000u;x->fw.buffers[i].bytes=32768;
    }
    for(i=0;i<16;i++) {
        x->r.buffer[i].cpu=aligned_alloc(4096,65536);assert(x->r.buffer[i].cpu);
        memset(x->r.buffer[i].cpu,0,65536);x->r.buffer[i].bytes=65536;
        x->r.buffer[i].logical.QuadPart=0x10000000u+i*0x100000u;x->r.buffer[i].adapter=&x->adapter;
    }
    x->fw.adapter=&x->adapter;x->fw.staged=x->fw.uncached=TRUE;x->fw.render_owner=&x->r;
    x->r.firmware=&x->fw;x->r.mapped=x->r.clear_ready=TRUE;x->r.screen_surface=&x->screen;
    pc=x->r.buffer[1].cpu;
    pc[A04_NATIVE_DATA_VA>>30]=(uint32_t)((uint64_t)x->r.buffer[2].logical.QuadPart>>8)|1u;
    pc[A04_NATIVE_USC_VA>>30]=(uint32_t)((uint64_t)x->r.buffer[8].logical.QuadPart>>8)|1u;
    pc[A04_NATIVE_PDS_VA>>30]=(uint32_t)((uint64_t)x->r.buffer[5].logical.QuadPart>>8)|1u;
    pc[512]=0x191001u; /* Existing screen PC index must remain untouched. */
    pd=x->r.buffer[8].cpu;pd[0]=(uint64_t)x->r.buffer[9].logical.QuadPart|1u;
    pt=x->r.buffer[9].cpu;for(i=0;i<16;i++)pt[i]=0x11000001u+(uint64_t)i*4096u;
    pd=x->r.buffer[5].cpu;pd[0]=(uint64_t)x->r.buffer[6].logical.QuadPart|1u;
    pt=x->r.buffer[6].cpu;for(i=0;i<16;i++)pt[i]=0x12000001u+(uint64_t)i*4096u;
    memset(x->r.buffer[7].cpu,0xa5,65536);memset(x->r.buffer[10].cpu,0x5a,65536);
    memcpy(x->original_pc,pc,4096);
}
static void preserved(struct fixture *x,int output)
{
    unsigned i;uint32_t *pc=x->r.buffer[1].cpu;
    for(i=0;i<1024;i++)if(i!=(A04_NATIVE_OUTPUT_VA>>30) || !output)assert(pc[i]==x->original_pc[i]);
    for(i=0;i<65536;i++) {
        assert(((unsigned char *)x->r.buffer[7].cpu)[i]==0xa5);
        assert(((unsigned char *)x->r.buffer[10].cpu)[i]==0x5a);
    }
    assert(x->r.screen_surface==&x->screen && !x->screen.prepared && !x->screen.pixels.cpu);
}
static void finish(struct fixture *x)
{
    unsigned i;assert(!x->r.native_owner && !pools);
    for(i=0;i<5;i++)free(x->fw.buffers[i].cpu);
    for(i=0;i<16;i++)free(x->r.buffer[i].cpu);
}
int main(void)
{
    struct fixture x;struct a04_vendor_native_cold *n=NULL;unsigned i;
    init(&x);
    assert(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,1100u,800u,1280u)==STATUS_SUCCESS);
    assert(n && n->backing_staged && n->output_mapped && n->code_mapped==3u);
    assert(n->output[0].bytes==4096000u && n->used==0x159000u && n->suffix_offset==0x158000u);
    preserved(&x,1);
    assert(((uint32_t *)x.r.buffer[1].cpu)[A04_NATIVE_OUTPUT_VA>>30]==
        ((uint32_t)((uint64_t)n->output[1].logical.QuadPart>>8)|1u));
    assert(((uint64_t *)n->output[1].cpu)[0]==((uint64_t)n->output[2].logical.QuadPart|1u));
    assert(((uint64_t *)n->output[1].cpu)[1]==((uint64_t)n->output[3].logical.QuadPart|1u));
    assert(((uint64_t *)n->pt.cpu)[((A04_NATIVE_DATA_VA+n->suffix_offset)>>12)&511u]==
        (((uint64_t)n->data.logical.QuadPart+n->suffix_offset)|A04_GPU_MMU_SLC_BYPASS|1u));
    assert(((uint64_t *)n->pt.cpu)[n->pm_offset[0]/4096u]&A04_GPU_MMU_PM_FW_PROTECT);
    {
        _Alignas(8) struct a04_m111_terminate_stream suffix;
        _Alignas(8) struct a04_m111_clear_codespace code;
        assert(!a04_m111_build_clear_terminate_stream(&suffix,A04_NATIVE_DATA_VA+n->suffix_offset,800,1280));
        assert(!memcmp((UCHAR *)n->data.cpu+n->suffix_offset,suffix.data,sizeof(suffix.data)));
        assert(!a04_m111_build_clear_codespace(&code,A04_NATIVE_USC_VA,A04_NATIVE_PDS_VA,0,n->clear_surface.emit[0],n->clear_surface.emit[1]));
        assert(!memcmp(n->code[0].cpu,code.usc,sizeof(code.usc)) && !memcmp(n->code[1].cpu,code.pds,sizeof(code.pds)));
        assert(n->bindings.pixel_event_info==0x202 && n->bindings.pixel_event_code==0x100a0u);
        assert(((uint64_t *)n->output[2].cpu)[511] && ((uint64_t *)n->output[3].cpu)[487]);
        assert(!((uint64_t *)n->output[3].cpu)[488]);
        for(i=0;i<4096000u/4u;i++)assert(!((uint32_t *)n->output[0].cpu)[i]);
    }
    x.fw.hardware_owned=TRUE;i=frees;
    assert(a04_vendor_native_release_cold(&n)==STATUS_DEVICE_BUSY && n && frees==i);
    x.fw.hardware_owned=FALSE;
    /* A changed catalogue entry is not ours to clear; retain everything. */
    ((uint32_t *)x.r.buffer[1].cpu)[A04_NATIVE_OUTPUT_VA>>30]^=2u;
    assert(!NT_SUCCESS(a04_vendor_native_release_cold(&n)) && n && frees==i);
    ((uint32_t *)x.r.buffer[1].cpu)[A04_NATIVE_OUTPUT_VA>>30]^=2u;
    /* Foreign code/data entries also block cleanup rather than overwrite. */
    ((uint64_t *)x.r.buffer[9].cpu)[16]^=0x1000u;
    assert(!NT_SUCCESS(a04_vendor_native_release_cold(&n)) && n && frees==i);
    ((uint64_t *)x.r.buffer[9].cpu)[16]^=0x1000u;
    ((uint64_t *)x.r.buffer[2].cpu)[(A04_NATIVE_DATA_VA>>21)&511u]^=0x1000u;
    assert(!NT_SUCCESS(a04_vendor_native_release_cold(&n)) && n && frees==i);
    ((uint64_t *)x.r.buffer[2].cpu)[(A04_NATIVE_DATA_VA>>21)&511u]^=0x1000u;
    /* Fail the final bank-zero workspace: mappings removed, owner retained. */
    fail_pool=pool_calls+1;
    assert(a04_vendor_native_release_cold(&n)==STATUS_INSUFFICIENT_RESOURCES && n && frees==i && x.r.native_owner==n);
    fail_pool=0;assert(a04_vendor_native_release_cold(&n)==STATUS_SUCCESS && !n && frees==8u);
    preserved(&x,0);finish(&x);
    for(i=1;i<=8;i++) {
        init(&x);fail_at=i;
        assert(!NT_SUCCESS(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,1100,800,1280)));
        assert(!n && frees==i-1u);preserved(&x,0);finish(&x);
    }
    for(i=1;i<=5;i++) {
        init(&x);fail_pool=i;
        assert(!NT_SUCCESS(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,1100,800,1280)));
        assert(!n && frees==allocs);preserved(&x,0);finish(&x);
    }
    init(&x);((uint32_t *)x.r.buffer[1].cpu)[A04_NATIVE_OUTPUT_VA>>30]=0xabcdef01u;
    assert(!NT_SUCCESS(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,1100,800,1280)) && !n);
    assert(((uint32_t *)x.r.buffer[1].cpu)[A04_NATIVE_OUTPUT_VA>>30]==0xabcdef01u);
    ((uint32_t *)x.r.buffer[1].cpu)[A04_NATIVE_OUTPUT_VA>>30]=0;preserved(&x,0);finish(&x);
    for(i=1;i<=8;i++) {
        pid_t child=fork();int status;assert(child>=0);
        if(!child) {
            init(&x);alias_at=i;
            assert(!NT_SUCCESS(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,1100,800,1280)));
            assert(n && n->quarantined && x.r.native_owner==n && !frees);
            assert(a04_vendor_native_release_cold(&n)==STATUS_DEVICE_BUSY && !frees);
            _exit(0); /* Quarantine intentionally retains this child process's backing. */
        }
        assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
    puts("Native cold output/code/suffix ownership tests passed");return 0;
}
