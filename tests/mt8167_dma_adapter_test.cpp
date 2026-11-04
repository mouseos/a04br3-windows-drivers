#include <cassert>
#include "dma_adapter.h"
int test_mdl_fail=0,test_mdl_frees=0,test_barriers=0,test_flushes=0;
_KINTERRUPT test_interrupt{};PDMA_ADAPTER test_dma_adapter=nullptr;DEVICE_DESCRIPTION test_dma_description{};
static int released;
static void put(PDMA_ADAPTER){released++;}
static PVOID alloc(PDMA_ADAPTER,ULONG,PHYSICAL_ADDRESS*,BOOLEAN){return nullptr;}
static void freebuf(PDMA_ADAPTER,ULONG,PHYSICAL_ADDRESS,PVOID,BOOLEAN){}
int main(){
 _DMA_OPERATIONS ops{put,alloc,freebuf};_DMA_ADAPTER adapter{&ops};PDMA_ADAPTER got=nullptr;ULONG maps=0;
 assert(Mt8167GetDmaAdapter(nullptr,&got,&maps)==STATUS_INVALID_PARAMETER);
 assert(Mt8167GetDmaAdapter(&adapter,&got,&maps)==STATUS_INSUFFICIENT_RESOURCES && !got);
 test_dma_adapter=&adapter;ops.FreeCommonBuffer=nullptr;
 assert(Mt8167GetDmaAdapter(&adapter,&got,&maps)==STATUS_NOT_SUPPORTED && !got && released==1);
 ops.FreeCommonBuffer=freebuf;assert(!Mt8167GetDmaAdapter(&adapter,&got,&maps));
 assert(got==&adapter && maps==4 && test_dma_description.Master && test_dma_description.Dma32BitAddresses && !test_dma_description.ScatterGather && test_dma_description.MaximumLength==1048576);
}
