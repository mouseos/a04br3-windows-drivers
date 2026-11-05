#include <cassert>
#include "pnp_backend.h"
#include "stream_transport.h"
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
#include "audio_parent_query.h"
static int parent_refs,query_failure,malformed;
static void parent_ref(PVOID){parent_refs++;}
static void parent_deref(PVOID){parent_refs--;}
static int parent_acquire(PVOID,ULONG){return -1;}
static int parent_release(PVOID){return -1;}
extern "C" NTSTATUS Mt8167QueryAudioParent(PDEVICE_OBJECT p,A04_SHARED_AUDIO_INTERFACE *out) {
 if(query_failure)return STATUS_NOT_SUPPORTED;
 *out={};out->Header.Size=sizeof(*out);out->Header.Version=1;out->Header.Context=p;
 out->Header.InterfaceReference=parent_ref;out->Header.InterfaceDereference=parent_deref;
 out->AcquireMuted=malformed?nullptr:parent_acquire;out->ReleaseQuiescent=parent_release;parent_refs++;return STATUS_SUCCESS;
}
#endif
int test_mdl_fail=0,test_mdl_frees=0,test_barriers=0,test_flushes=0,test_pdo_refs=0;
_KINTERRUPT test_interrupt{};PDMA_ADAPTER test_dma_adapter=nullptr;DEVICE_DESCRIPTION test_dma_description{};
static int shared_refs;
static int shared_ref(void *){shared_refs++;return 0;}
static void shared_deref(void *){shared_refs--;}
static int shared_acquire(void *,uint32_t){return -1;}
static int shared_release(void *){return 0;}
static int freed,puts; static unsigned char data[8192];
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
static int maps,unmaps,mapfail;static unsigned char mmio[4096];
PVOID MmMapIoSpace(PHYSICAL_ADDRESS a,ULONG n,MEMORY_CACHING_TYPE cache) { assert(a.QuadPart==0x11140000 && n==4096 && cache==MmNonCached);maps++;return mapfail?nullptr:mmio; }
void MmUnmapIoSpace(PVOID p,ULONG n) { assert(p==mmio && n==4096);unmaps++; }
#endif
static void put(PDMA_ADAPTER){puts++;}
static PVOID alloc(PDMA_ADAPTER,ULONG n,PHYSICAL_ADDRESS *l,BOOLEAN){assert(n==8192);l->QuadPart=0x100000;return data;}
static void freebuf(PDMA_ADAPTER,ULONG,PHYSICAL_ADDRESS,PVOID,BOOLEAN){freed++;}
struct Resources : IResourceList { _CM_PARTIAL_RESOURCE_DESCRIPTOR irq{},rawIrq{},mem[2]{};
 Resources(){irq.ShareDisposition=rawIrq.ShareDisposition=CmResourceShareDeviceExclusive;rawIrq.u.Interrupt.Vector=152;irq.u.Interrupt.Vector=80;irq.u.Interrupt.Level=5;irq.u.Interrupt.Affinity=1;mem[0].ShareDisposition=mem[1].ShareDisposition=CmResourceShareDeviceExclusive;mem[0].u.Memory.Start.QuadPart=0x11140000;mem[0].u.Memory.Length=0x1000;mem[1].u.Memory.Start.QuadPart=0x11141000;mem[1].u.Memory.Length=0x9000;}
 ULONG NumberOfEntriesOfType(int type) override{return type==CmResourceTypeMemory?2:1;}
 PCM_PARTIAL_RESOURCE_DESCRIPTOR FindTranslatedEntry(int type,ULONG index) override{return type==CmResourceTypeMemory?(index<2?&mem[index]:nullptr):&irq;}
 PCM_PARTIAL_RESOURCE_DESCRIPTOR FindUntranslatedEntry(int type,ULONG index) override{return type==CmResourceTypeInterrupt && !index?&rawIrq:nullptr;}
};
int main(){
 _DMA_OPERATIONS ops{put,alloc,freebuf};_DMA_ADAPTER adapter{&ops};test_dma_adapter=&adapter;
 Resources resources;Mt8167PnpBackend *owner=nullptr;
 resources.rawIrq.Flags=CM_RESOURCE_INTERRUPT_MESSAGE;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner);
 resources.rawIrq.Flags=0;resources.rawIrq.ShareDisposition=0;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner);
 resources.rawIrq.ShareDisposition=CmResourceShareDeviceExclusive;resources.rawIrq.u.Interrupt.Vector=0;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner);
 resources.rawIrq.u.Interrupt.Vector=152;
 resources.irq.Flags=CM_RESOURCE_INTERRUPT_LATCHED;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner);
 resources.irq.Flags=CM_RESOURCE_INTERRUPT_MESSAGE;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner);
 resources.irq.Flags=0;resources.irq.u.Interrupt.Vector=0;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner);
 resources.irq.u.Interrupt.Vector=80;resources.irq.u.Interrupt.Level=32;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner);
 resources.irq.u.Interrupt.Level=5;
#ifdef NT_PROCESSOR_GROUPS
 resources.irq.u.Interrupt.Group=1;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner);resources.irq.u.Interrupt.Group=0;
#endif
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
 query_failure=1;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_NOT_SUPPORTED && !owner && puts==1 && !parent_refs);
 query_failure=0;malformed=1;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner && puts==2 && !parent_refs);
 malformed=0;puts=0;
#endif
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
 resources.mem[0].ShareDisposition=0;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner && !maps && !parent_refs);
 resources.mem[0].ShareDisposition=CmResourceShareDeviceExclusive;
 resources.mem[1].u.Memory.Length=0x8000;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner && !maps && !parent_refs);
 resources.mem[1].u.Memory.Length=0x9000;mapfail=1;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_INSUFFICIENT_RESOURCES && !owner && maps==1 && !unmaps && !parent_refs);mapfail=0;puts=0;
 _CM_PARTIAL_RESOURCE_DESCRIPTOR swap=resources.mem[0];resources.mem[0]=resources.mem[1];resources.mem[1]=swap;
#endif
 assert(!Mt8167PnpBackendCreate(&adapter,&resources,&owner) && owner && test_pdo_refs==1);
 Mt8167PnpInterrupt irq{};assert(!Mt8167PnpBackendInterrupt(owner,&irq) && irq.vector==80 && irq.level==5 && irq.affinity==1);
 Mt8167Irq irqo{};Mt8167Transport transport{};transport.irq=&irqo;
 assert(!Mt8167PnpBackendBindInterrupt(owner,&transport) && transport.vector==80 && transport.level==5 && transport.affinity==1);
 transport.state=AUDIO_RUNNING;assert(Mt8167PnpBackendBindInterrupt(owner,&transport)==STATUS_DEVICE_NOT_READY);transport.state=AUDIO_READY;
 mt8167_audio_shared shared{sizeof(shared),1,&adapter,shared_ref,shared_deref,shared_acquire,shared_release};
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
 assert(parent_refs==2);
 assert(Mt8167PnpBackendAttachShared(owner,&shared)==STATUS_DEVICE_NOT_READY && !shared_refs);
#else
 assert(!Mt8167PnpBackendAttachShared(owner,&shared) && shared_refs==1);
#endif
 assert(Mt8167PnpBackendAttachShared(owner,&shared)==STATUS_DEVICE_NOT_READY);
 const Mt8167StreamBackend *api=Mt8167PnpBackendInterface(owner);api->reference(api->context);
 PMDL mdl=nullptr;ULONG n=0,offset=9;MEMORY_CACHING_TYPE cache;
 assert(!api->allocate(api->context,8192,&mdl,&n,&offset,&cache) && mdl && n==8192 && !offset);
 BOOLEAN exposure=TRUE;assert(api->start(api->context,&exposure)==STATUS_DEVICE_NOT_READY && !exposure);
 Mt8167PnpBackendRevoke(owner);assert(Mt8167PnpBackendAttachShared(owner,&shared)==STATUS_DEVICE_NOT_READY);assert(Mt8167PnpBackendInterrupt(owner,&irq)==STATUS_DEVICE_NOT_READY);Mt8167PnpBackendRelease(owner);
 assert(Mt8167PnpBackendBindInterrupt(owner,&transport)==STATUS_DEVICE_NOT_READY);
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
 assert(maps==2 && unmaps==1);
#endif
 PMDL second=nullptr;assert(api->allocate(api->context,8192,&second,&n,&offset,&cache)==STATUS_DEVICE_NOT_READY && !second);
 assert(!freed && !puts && test_pdo_refs==1);
 api->freeBuffer(api->context,mdl,8192);assert(freed==1 && test_mdl_frees==1);
 api->dereference(api->context);assert(puts==1 && test_pdo_refs==0);
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
 assert(!parent_refs);
#endif
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
 assert(unmaps==1);
#endif
}
