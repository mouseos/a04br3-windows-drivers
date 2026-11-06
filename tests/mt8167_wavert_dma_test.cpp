#include <cassert>
#include <cstring>
#include "wavert_dma.h"
struct Port : IPortWaveRTStream {
 MDL m{0,8192}; unsigned char data[8192]; ULONG pages=2; int64_t base=0x100000, second=0x101000; bool map=true; int freed=0, unmapped=0, refs=1;
 PMDL AllocateContiguousPagesForMdl(PHYSICAL_ADDRESS,PHYSICAL_ADDRESS,size_t) override { return &m; }
 ULONG GetPhysicalPagesCount(PMDL) override { return pages; }
 PHYSICAL_ADDRESS GetPhysicalPageAddress(PMDL,ULONG i) override { return {i ? second : base}; }
 PVOID MapAllocatedPages(PMDL,int) override { return map ? data : nullptr; }
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
 assert(Mt8167AllocateBuffer(&p,0,&b)==STATUS_INVALID_PARAMETER);
 assert(Mt8167AllocateBuffer(&p,1048577,&b)==STATUS_INVALID_PARAMETER);
}
