/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include "windows_uncached.h"
static unsigned allocated,freed;static int bad_address;
unsigned KeGetCurrentIrql(void) { return PASSIVE_LEVEL; }
static void *make_buffer(PDMA_ADAPTER a,PHYSICAL_ADDRESS *max,ULONG n,PHYSICAL_ADDRESS *pa,BOOLEAN cache,ULONG node)
{
    (void)a;assert(max->QuadPart==0xffffffffLL && !cache && !node);
    ++allocated;pa->QuadPart=bad_address?0xfffff000LL:0x80000000LL;return malloc(n);
}
static void free_buffer(PDMA_ADAPTER a,ULONG n,PHYSICAL_ADDRESS pa,void *cpu,BOOLEAN cache)
{ (void)a;(void)n;(void)pa;assert(!cache);++freed;free(cpu); }
int main(void)
{
    DMA_OPERATIONS ops={.FreeCommonBuffer=free_buffer,.Size=sizeof(ops),.AllocateCommonBufferEx=make_buffer};
    struct fake_adapter adapter={&ops};struct a04_vendor_uncached b={0};
    ops.Size=0;assert(a04_vendor_uncached_allocate(&b,&adapter,8192)==STATUS_NOT_SUPPORTED && !allocated);
    ops.Size=sizeof(ops);bad_address=1;
    assert(a04_vendor_uncached_allocate(&b,&adapter,8192)==STATUS_DEVICE_CONFIGURATION_ERROR && freed==1 && !b.cpu);
    bad_address=0;assert(a04_vendor_uncached_allocate(&b,&adapter,8192)==STATUS_SUCCESS && b.cpu && b.bytes==8192);
    assert(a04_vendor_uncached_allocate(&b,&adapter,8192)==STATUS_INVALID_PARAMETER);
    a04_vendor_uncached_free(&b);assert(freed==2 && !b.cpu && !b.adapter);return 0;
}
