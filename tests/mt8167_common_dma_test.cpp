#include <cassert>
#include <cstring>
#include "common_dma.h"
int test_mdl_fail=0,test_mdl_frees=0,test_barriers=0,test_flushes=0;
_KINTERRUPT test_interrupt{};
static unsigned char memory[8192];
static int64_t address=0x100000;static int alloc_fail,free_count;
static PVOID alloc(PDMA_ADAPTER,ULONG n,PHYSICAL_ADDRESS *p,BOOLEAN cache) {assert(n==8192 && !cache);if(alloc_fail)return nullptr;p->QuadPart=address;return memory;}
static void release(PDMA_ADAPTER,ULONG n,PHYSICAL_ADDRESS p,PVOID q,BOOLEAN cache) {assert(n==8192 && p.QuadPart==address && q==memory && !cache);free_count++;}
int main(){
 _DMA_OPERATIONS ops{nullptr,alloc,release};_DMA_ADAPTER adapter{&ops};Mt8167CommonDma b{};
 assert(Mt8167CommonAllocate(&adapter,4097,&b)==STATUS_INVALID_PARAMETER && !free_count);
 std::memset(memory,0xff,sizeof memory);assert(!Mt8167CommonAllocate(&adapter,8192,&b));
 assert(b.logical.QuadPart==0x100000 && b.mdl && !memory[8191] && test_barriers==1);
 assert(!Mt8167CommonReleaseUnexposed(&b) && free_count==1 && test_mdl_frees==1 && !b.cpu);
 address=0xfffff000LL;assert(Mt8167CommonAllocate(&adapter,8192,&b)==STATUS_DEVICE_CONFIGURATION_ERROR && free_count==2);
 address=0x100000;test_mdl_fail=1;assert(Mt8167CommonAllocate(&adapter,8192,&b)==STATUS_DEVICE_CONFIGURATION_ERROR && free_count==3);test_mdl_fail=0;
 assert(!Mt8167CommonAllocate(&adapter,8192,&b));assert(Mt8167CommonMarkExposed(&b)==STATUS_INVALID_PARAMETER);
 assert(!Mt8167CommonPrepareForDevice(&b) && test_flushes==1);assert(!Mt8167CommonMarkExposed(&b));
 assert(Mt8167CommonPrepareForDevice(&b)==STATUS_INVALID_PARAMETER);
 assert(Mt8167CommonMarkExposed(&b)==STATUS_INVALID_PARAMETER);
 assert(Mt8167CommonReleaseUnexposed(&b)==STATUS_DEVICE_BUSY && free_count==3 && test_mdl_frees==1);
 /* Test-only cleanup; no hardware has been used by the mock. */
 IoFreeMdl(b.mdl);
}
