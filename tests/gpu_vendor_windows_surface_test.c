/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "windows_surface.h"
static unsigned allocs,frees,pools;
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
void a04_gpu_dma_barrier(void){}
void *ExAllocatePoolWithTag(int t,size_t n,unsigned tag){(void)t;(void)tag;void *p=aligned_alloc(4096,(n+4095)&~(size_t)4095);if(p)pools++;return p;}
void ExFreePoolWithTag(void *p,unsigned tag){(void)tag;pools--;free(p);}
static void *alloc(PDMA_ADAPTER a,PHYSICAL_ADDRESS *m,ULONG n,PHYSICAL_ADDRESS *p,BOOLEAN c,ULONG node)
{(void)a;(void)m;(void)c;(void)node;allocs++;p->QuadPart=0x40000000LL+(int64_t)allocs*0x01000000LL;return aligned_alloc(4096,(n+4095)&~4095u);}
static void release(PDMA_ADAPTER a,ULONG n,PHYSICAL_ADDRESS p,void *v,BOOLEAN c)
{(void)a;(void)n;(void)p;(void)c;frees++;free(v);}
int main(void)
{
    DMA_OPERATIONS ops={.Size=sizeof(ops),.AllocateCommonBufferEx=alloc,.FreeCommonBuffer=release};
    struct fake_adapter adapter={&ops};struct a04_vendor_surface s={0};
    assert(a04_vendor_surface_prepare(&s,&adapter,800,1280,3328,UINT64_C(0x8000000000),1039)==STATUS_INSUFFICIENT_RESOURCES);
    assert(!allocs && !pools);
    assert(a04_vendor_surface_prepare(&s,&adapter,800,1280,3328,UINT64_C(0x8000000000),1040)==STATUS_SUCCESS);
    assert(s.prepared && s.bytes==4259840 && s.leaves==3 && allocs==6 && !pools);
    assert(((uint64_t *)s.pt[0].cpu)[0]==((uint64_t)s.pixels.logical.QuadPart|9u));
    assert(a04_vendor_surface_release(&s)==STATUS_SUCCESS && frees==6);
    assert(a04_vendor_surface_prepare(&s,&adapter,800,1280,3328,UINT64_C(0x8000000000),1040)==STATUS_SUCCESS);
    assert(a04_vendor_surface_mark_exposed(&s)==STATUS_SUCCESS);
    assert(a04_vendor_surface_release(&s)==STATUS_DEVICE_BUSY && frees==6);
    /* Test process exits with mocked retained allocations, like the real gate. */
    return 0;
}
