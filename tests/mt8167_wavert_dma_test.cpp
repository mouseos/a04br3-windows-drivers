#include <cassert>
#include <cstring>
#include "wavert_dma.h"
#include "wavert_cache.h"
static uint64_t testPar;
NTSTATUS Mt8167VerifyWaveCache(PVOID,ULONG bytes,ULONG first) {
 for(ULONG i=0;i<bytes;i+=PAGE_SIZE)
  if(!mt8167_arm_par_normal_nc(testPar | (first+i),first+i))return STATUS_DEVICE_CONFIGURATION_ERROR;
 return STATUS_SUCCESS;
}
struct Port : IPortWaveRTStream {
 MDL m{0,8192}; unsigned char data[8192]; ULONG pages=2; int64_t base=0x100000, second=0x101000; bool map=true; int freed=0, unmapped=0, refs=1;
 PMDL AllocateContiguousPagesForMdl(PHYSICAL_ADDRESS,PHYSICAL_ADDRESS,size_t) override { return &m; }
 ULONG GetPhysicalPagesCount(PMDL) override { return pages; }
 PHYSICAL_ADDRESS GetPhysicalPageAddress(PMDL,ULONG i) override { return {i ? second : base}; }
 PVOID MapAllocatedPages(PMDL,int cache) override { assert(cache==MmWriteCombined);return map ? data : nullptr; }
 void UnmapAllocatedPages(PVOID,PMDL) override { unmapped++; }
 void FreePagesFromMdl(PMDL) override { freed++; }
 ULONG AddRef() override { return ++refs; }
 ULONG Release() override { return --refs; }
};
int main() {
 Port p; Mt8167WaveBuffer b{}; std::memset(p.data,0xff,sizeof p.data);
 assert(Mt8167AllocateBuffer(&p,8192,&b)==STATUS_SUCCESS); assert(b.physicalAddress==0x100000 && p.refs==2 && !p.data[8191]);
 assert(Mt8167ReleaseUnpublishedBuffer(&b)==STATUS_SUCCESS && p.freed==1 && p.unmapped==1 && p.refs==1);
 { Port active; Mt8167WaveBuffer live{}; assert(Mt8167AllocateBuffer(&active,8192,&live)==STATUS_SUCCESS);
   live.handedToDevice=TRUE; assert(Mt8167ReleaseUnpublishedBuffer(&live)==STATUS_DEVICE_BUSY && !active.freed); }
 p.second=0x102000; assert(Mt8167AllocateBuffer(&p,8192,&b)==STATUS_DEVICE_CONFIGURATION_ERROR && p.freed==2 && !b.mdl);
 p.second=0x101000; p.base=0xfffff000LL; assert(Mt8167AllocateBuffer(&p,8192,&b)==STATUS_DEVICE_CONFIGURATION_ERROR);
 p.base=0x100000; p.map=false; assert(Mt8167AllocateBuffer(&p,8192,&b)==STATUS_DEVICE_CONFIGURATION_ERROR);
 p.map=true; assert(Mt8167AllocateBuffer(&p,4097,&b)==STATUS_SUCCESS && b.bytes==8192);
 assert(Mt8167ReleaseUnpublishedBuffer(&b)==STATUS_SUCCESS);
 /* ARM architectural attribute decoder: same physical page is insufficient. */
 assert(mt8167_arm_par_normal_nc(0x100000,0x100000)); /* short Normal NC */
 assert(mt8167_arm_par_normal_nc(0x4400000000100800ULL,0x100000)); /* long Normal NC */
 assert(!mt8167_arm_par_normal_nc(0x100030,0x100000)); /* short Device */
 assert(!mt8167_arm_par_normal_nc(0x100010,0x100000)); /* Strongly-ordered */
 assert(!mt8167_arm_par_normal_nc(0x100070,0x100000)); /* cached */
 assert(!mt8167_arm_par_normal_nc(0x0000000000100800ULL,0x100000)); /* long Device */
 assert(!mt8167_arm_par_normal_nc(0xff00000000100800ULL,0x100000)); /* long WB */
 assert(!mt8167_arm_par_normal_nc(0x100001,0x100000)); /* translation fault */
 assert(!mt8167_arm_par_normal_nc(0x200000,0x100000)); /* different page */
 testPar=0x30;std::memset(p.data,0xa5,sizeof p.data);
 int oldUnmapped=p.unmapped;
 assert(Mt8167AllocateBuffer(&p,8192,&b)==STATUS_DEVICE_CONFIGURATION_ERROR);
 assert(p.unmapped==oldUnmapped+1 && !b.mdl && p.data[0]==0xa5 && p.data[8191]==0xa5);
 testPar=0;
 assert(Mt8167AllocateBuffer(&p,0,&b)==STATUS_INVALID_PARAMETER);
 assert(Mt8167AllocateBuffer(&p,1048577,&b)==STATUS_INVALID_PARAMETER);
}
