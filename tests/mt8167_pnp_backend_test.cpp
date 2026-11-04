#include <cassert>
#include "pnp_backend.h"
int test_mdl_fail=0,test_mdl_frees=0,test_barriers=0,test_flushes=0,test_pdo_refs=0;
_KINTERRUPT test_interrupt{};PDMA_ADAPTER test_dma_adapter=nullptr;DEVICE_DESCRIPTION test_dma_description{};
static int freed,puts; static unsigned char data[8192];
static void put(PDMA_ADAPTER){puts++;}
static PVOID alloc(PDMA_ADAPTER,ULONG n,PHYSICAL_ADDRESS *l,BOOLEAN){assert(n==8192);l->QuadPart=0x100000;return data;}
static void freebuf(PDMA_ADAPTER,ULONG,PHYSICAL_ADDRESS,PVOID,BOOLEAN){freed++;}
struct Resources : IResourceList { _CM_PARTIAL_RESOURCE_DESCRIPTOR irq{};
 Resources(){irq.ShareDisposition=CmResourceShareDeviceExclusive;irq.u.Interrupt.Vector=152;irq.u.Interrupt.Level=5;irq.u.Interrupt.Affinity=1;}
 ULONG NumberOfEntriesOfType(int) override{return 1;}
 PCM_PARTIAL_RESOURCE_DESCRIPTOR FindTranslatedEntry(int,ULONG) override{return &irq;}
};
int main(){
 _DMA_OPERATIONS ops{put,alloc,freebuf};_DMA_ADAPTER adapter{&ops};test_dma_adapter=&adapter;
 Resources resources;Mt8167PnpBackend *owner=nullptr;
 resources.irq.Flags=CM_RESOURCE_INTERRUPT_LATCHED;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner);
 resources.irq.Flags=0;assert(!Mt8167PnpBackendCreate(&adapter,&resources,&owner) && owner && test_pdo_refs==1);
 Mt8167PnpInterrupt irq{};assert(!Mt8167PnpBackendInterrupt(owner,&irq) && irq.vector==152 && irq.level==5 && irq.affinity==1);
 const Mt8167StreamBackend *api=Mt8167PnpBackendInterface(owner);api->reference(api->context);
 PMDL mdl=nullptr;ULONG n=0,offset=9;MEMORY_CACHING_TYPE cache;
 assert(!api->allocate(api->context,8192,&mdl,&n,&offset,&cache) && mdl && n==8192 && !offset);
 assert(api->start(api->context)==STATUS_DEVICE_NOT_READY);
 Mt8167PnpBackendRevoke(owner);assert(Mt8167PnpBackendInterrupt(owner,&irq)==STATUS_DEVICE_NOT_READY);Mt8167PnpBackendRelease(owner);
 PMDL second=nullptr;assert(api->allocate(api->context,8192,&second,&n,&offset,&cache)==STATUS_DEVICE_NOT_READY && !second);
 assert(!freed && !puts && test_pdo_refs==1);
 api->freeBuffer(api->context,mdl,8192);assert(freed==1 && test_mdl_frees==1);
 api->dereference(api->context);assert(puts==1 && test_pdo_refs==0);
}
