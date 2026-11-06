#include <cassert>
#include <atomic>
#include <thread>
#include "pnp_backend.h"
#include "stream_transport.h"
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
#include "audio_parent_query.h"
static int parent_refs,query_failure,malformed;
static int parent_grant,parent_prepare_fail,parent_prepares,parent_releases,parent_release_fail;
static PVOID parent_afe;
static int parent_mute(PVOID){return 0;}
static int parent_unmutes;
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
static int live_verified,voice_starts,voice_stops;
static int verify_pcm(PVOID,PVOID mdl,ULONGLONG logical,ULONG size){return !(live_verified && mdl && logical==0x100000 && size==8192);}
static int voice_start(PVOID){voice_starts++;
#if defined(A04_TEST_VOICE_FAILURE) && A04_TEST_VOICE_FAILURE
 return -1;
#else
 return 0;
#endif
}
static int voice_stop(PVOID){voice_stops++;return 0;}
#endif
static int parent_unmute(PVOID){parent_unmutes++;
#if defined(A04_TEST_UNMUTE_FAILURE) && A04_TEST_UNMUTE_FAILURE
 return -1;
#else
 return 0;
#endif
}
static int parent_bind(PVOID,PVOID afe,ULONG n){if(afe && n==4096 && !parent_afe){parent_afe=afe;return 0;}if(!afe && !n && parent_afe){parent_afe=nullptr;return 0;}return -1;}
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
int test_connect_fail,test_irq_connections,test_irq_disconnections;
void test_before_disconnect(){assert(!parent_releases);}
#endif
static void parent_ref(PVOID){parent_refs++;}
static void parent_deref(PVOID){parent_refs--;}
static int parent_acquire(PVOID,ULONG){return parent_grant?0:-1;}
static int parent_release(PVOID){parent_releases++;return parent_release_fail?-1:0;}
static int parent_prepare(PVOID,PVOID afe,ULONG n){assert(afe && n==4096);parent_prepares++;return parent_prepare_fail?-1:0;}
extern "C" NTSTATUS Mt8167QueryAudioParent(PDEVICE_OBJECT p,A04_SHARED_AUDIO_INTERFACE *out) {
 if(query_failure)return STATUS_NOT_SUPPORTED;
 *out={};out->Header.Size=sizeof(*out);out->Header.Version=A04_SHARED_AUDIO_INTERFACE_VERSION;out->Header.Context=p;
 out->Header.InterfaceReference=parent_ref;out->Header.InterfaceDereference=parent_deref;
 #if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
 out->VerifyLivePcm=verify_pcm;out->StartVoice=voice_start;out->StopVoice=voice_stop;
#endif
 out->AcquireMuted=malformed?nullptr:parent_acquire;out->ReleaseQuiescent=parent_release;out->PrepareMaskedIrq=parent_prepare;out->BindAfe=parent_bind;out->Mute=parent_mute;out->UnmuteMin=parent_unmute;parent_refs++;return STATUS_SUCCESS;
}
#endif
#if defined(A04_AUDIO_DIAGNOSTIC_TRACE) && A04_AUDIO_DIAGNOSTIC_TRACE
int a04_test_trace_count=0;
#endif
thread_local int test_audio_irql=0;
int test_mdl_fail=0,test_mdl_frees=0,test_barriers=0,test_flushes=0,test_pdo_refs=0;
_KINTERRUPT test_interrupt{};PDMA_ADAPTER test_dma_adapter=nullptr;DEVICE_DESCRIPTION test_dma_description{};
static int shared_refs;
static int shared_ref(void *){shared_refs++;return 0;}
static void shared_deref(void *){shared_refs--;}
static int shared_acquire(void *,uint32_t){return -1;}
static int shared_release(void *){return 0;}
static int freed,puts; static unsigned char data[8192];
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
static unsigned char silentData[8192];static unsigned allocCalls;
#endif
static std::atomic<bool> holdPosition(false),enteredPosition(false),revokeDone(false);
#if defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
extern "C" int Mt8167SilentDmaMiniportEnabled(void){return 1;}
int Mt8167TransportStartMaskedParent(Mt8167Transport *t,uint32_t period,mt8167_audio_shared_client *s){
 assert(t && t->dma && t->dma->cpu && t->dma->logical.QuadPart==0x200000 && period==4096);
#if defined(A04_AUDIO_PCM_PATTERN_DIAGNOSTIC) && A04_AUDIO_PCM_PATTERN_DIAGNOSTIC
 auto pcm=(unsigned char *)t->dma->cpu;assert(pcm[0]==0 && pcm[1]==0 && pcm[4]==25 && pcm[6]==25 && pcm[32]==128 && pcm[33]==0 && pcm[64]==0 && pcm[65]==0 && pcm[96]==128 && pcm[97]==255);
#else
 for(unsigned i=0;i<t->dma->size;i++)assert(((unsigned char *)t->dma->cpu)[i]==0);
#endif
 t->dma->exposed=TRUE;s->quarantined=1;t->state=AUDIO_RUNNING;return 0;
}
int Mt8167TransportStop(Mt8167Transport *t){assert(t->state==AUDIO_RUNNING);t->state=AUDIO_QUARANTINED;return 0;}
#elif defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
int Mt8167TransportStartMaskedParent(Mt8167Transport *t,uint32_t period,mt8167_audio_shared_client *s){
 assert(t && t->dma && t->dma->cpu && t->dma->logical.QuadPart==0x100000 && period==4096 && voice_starts==1);
 t->dma->exposed=TRUE;s->quarantined=1;t->state=AUDIO_RUNNING;return 0;
}
int Mt8167TransportStop(Mt8167Transport *t){assert(t->state==AUDIO_RUNNING);t->state=AUDIO_QUARANTINED;return 0;}
#else
int Mt8167TransportStartMaskedParent(Mt8167Transport *,uint32_t,mt8167_audio_shared_client *){assert(!"live DMA guard failed");return -1;}
int Mt8167TransportStop(Mt8167Transport *){assert(!"unexpected stop");return -1;}
#endif
#endif
int Mt8167TransportPosition(Mt8167Transport *,uint32_t *){enteredPosition=true;while(holdPosition)std::this_thread::yield();return -1;}
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
static int maps,unmaps,mapfail;alignas(4) static unsigned char mmio[4096];
PVOID MmMapIoSpace(PHYSICAL_ADDRESS a,ULONG n,MEMORY_CACHING_TYPE cache) { assert(a.QuadPart==0x21140000 && n==4096 && cache==MmNonCached);maps++;return mapfail?nullptr:mmio; }
void MmUnmapIoSpace(PVOID p,ULONG n) { assert(p==mmio && n==4096);unmaps++; }
#endif
static void put(PDMA_ADAPTER){puts++;}
static PVOID alloc(PDMA_ADAPTER,ULONG n,PHYSICAL_ADDRESS *l,BOOLEAN){assert(test_audio_irql==0 && n==8192);
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
 if(allocCalls++){l->QuadPart=0x200000;return silentData;}
#endif
 l->QuadPart=0x100000;return data;}
static void freebuf(PDMA_ADAPTER,ULONG,PHYSICAL_ADDRESS,PVOID,BOOLEAN){assert(test_audio_irql==0);freed++;}
struct Resources : IResourceList { _CM_PARTIAL_RESOURCE_DESCRIPTOR irq{},rawIrq{},mem[2]{},rawMem[2]{};
 Resources(){irq.ShareDisposition=rawIrq.ShareDisposition=CmResourceShareDeviceExclusive;rawIrq.u.Interrupt.Vector=152;irq.u.Interrupt.Vector=80;irq.u.Interrupt.Level=5;irq.u.Interrupt.Affinity=1;rawMem[0].ShareDisposition=rawMem[1].ShareDisposition=CmResourceShareDeviceExclusive;rawMem[0].u.Memory.Start.QuadPart=0x11140000;rawMem[0].u.Memory.Length=0x1000;rawMem[1].u.Memory.Start.QuadPart=0x11141000;rawMem[1].u.Memory.Length=0x9000;mem[0]=rawMem[0];mem[1]=rawMem[1];mem[0].u.Memory.Start.QuadPart=0x21140000;mem[1].u.Memory.Start.QuadPart=0x21141000;}
 ULONG NumberOfEntriesOfType(int type) override{return type==CmResourceTypeMemory?2:1;}
 PCM_PARTIAL_RESOURCE_DESCRIPTOR FindTranslatedEntry(int type,ULONG index) override{return type==CmResourceTypeMemory?(index<2?&mem[index]:nullptr):&irq;}
 PCM_PARTIAL_RESOURCE_DESCRIPTOR FindUntranslatedEntry(int type,ULONG index) override{return type==CmResourceTypeInterrupt && !index?&rawIrq:type==CmResourceTypeMemory && index<2?&rawMem[index]:nullptr;}
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
 resources.mem[1].u.Memory.Length=0x9000;resources.mem[1].u.Memory.Start.QuadPart=0x21140800;
 assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner && !maps && !parent_refs);
 resources.mem[1].u.Memory.Start.QuadPart=0x21141000;resources.rawMem[0].u.Memory.Start.QuadPart=0x11150000;
 assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_CONFIGURATION_ERROR && !owner && !maps && !parent_refs);
 resources.rawMem[0].u.Memory.Start.QuadPart=0x11140000;mapfail=1;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_INSUFFICIENT_RESOURCES && !owner && maps==1 && !unmaps && !parent_refs);mapfail=0;puts=0;
 _CM_PARTIAL_RESOURCE_DESCRIPTOR swap=resources.mem[0];resources.mem[0]=resources.mem[1];resources.mem[1]=swap;
 swap=resources.rawMem[0];resources.rawMem[0]=resources.rawMem[1];resources.rawMem[1]=swap;
#endif
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
 assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_NOT_READY && !owner && !test_irq_connections && !parent_refs);
 parent_grant=1;parent_prepare_fail=1;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_NOT_READY && !owner && !test_irq_connections && parent_releases==1 && !parent_refs);
 parent_prepare_fail=0;test_connect_fail=1;assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)<0 && !owner && !test_irq_connections && parent_releases==2 && !parent_refs);
 test_connect_fail=0;parent_releases=0;puts=0;
#endif
 assert(!Mt8167PnpBackendCreate(&adapter,&resources,&owner) && owner && test_pdo_refs==1);
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
 assert(test_irq_connections==1 && test_interrupt.fn && !test_interrupt.fn(&test_interrupt,test_interrupt.ctx));
 { auto irqstate=static_cast<Mt8167Irq *>(test_interrupt.ctx);auto &io=irqstate->engine->io;uint32_t value=99;
   assert(io.context && io.read32 && io.write32);
   assert(!io.write32(io.context,0x40,0x12345678) && !io.read32(io.context,0x40,&value) && value==0x12345678);
   assert(io.write32(io.context,0x44,1) && io.write32(io.context,0x3a4,1));
   assert(io.read32(io.context,0x41,&value) && io.read32(io.context,0x1000,&value));
   assert(io.write32(io.context,0x100,1) && !io.read32(io.context,0x3a4,&value));
   *reinterpret_cast<uint32_t *>(mmio+0x3a4)=1;
   *reinterpret_cast<uint32_t *>(mmio+0x10)=2;
   irqstate->engine->state=MT8167_DL1_RUNNING;
   assert(test_interrupt.fn(&test_interrupt,test_interrupt.ctx));
   assert(*reinterpret_cast<uint32_t *>(mmio+0x3a8)==1 && irqstate->pending==1);
   irqstate->engine->state=MT8167_DL1_IDLE;
 }
#endif
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
#if defined(A04_AUDIO_VOICE_PCM_DIAGNOSTIC) && A04_AUDIO_VOICE_PCM_DIAGNOSTIC
 { _MDL portMdl{data,8192};BOOLEAN exposed=TRUE;
   assert(api->bindWaveBuffer && api->unbindWaveBuffer);
   assert(!api->bindWaveBuffer(api->context,&portMdl,data,8192));
   assert(api->bindWaveBuffer(api->context,&portMdl,data,8192)==STATUS_DEVICE_NOT_READY);
   live_verified=1;assert(api->start(api->context,&exposed)==STATUS_DEVICE_NOT_READY && !exposed && !voice_starts);
   assert(!api->unbindWaveBuffer(api->context,&portMdl));
   Mt8167PnpBackendRevoke(owner);Mt8167PnpBackendRelease(owner);api->dereference(api->context);
   assert(!freed && puts==1 && !parent_refs && !test_pdo_refs);return 0; }
#endif
 PMDL mdl=nullptr;ULONG n=0,offset=9;MEMORY_CACHING_TYPE cache;
 test_audio_irql=1;assert(api->allocate(api->context,8192,&mdl,&n,&offset,&cache)==STATUS_DEVICE_NOT_READY && !mdl);test_audio_irql=0;
 assert(!api->allocate(api->context,8192,&mdl,&n,&offset,&cache) && mdl && n==8192 && !offset);
 BOOLEAN exposure=TRUE;
 test_audio_irql=1;assert(api->start(api->context,&exposure)==STATUS_DEVICE_NOT_READY && !exposure);api->freeBuffer(api->context,mdl,8192);assert(!freed);test_audio_irql=0;
#if defined(A04_AUDIO_SILENT_DMA_DIAGNOSTIC) && A04_AUDIO_SILENT_DMA_DIAGNOSTIC
#if defined(A04_TEST_UNMUTE_FAILURE) && A04_TEST_UNMUTE_FAILURE
 assert(api->start(api->context,&exposure)==STATUS_DEVICE_NOT_READY && exposure && allocCalls==2 && parent_unmutes==1);
 Mt8167PnpBackendRevoke(owner);Mt8167PnpBackendRelease(owner);api->dereference(api->context);
 assert(!freed && !puts && parent_refs==2 && test_pdo_refs==1);return 0;
#else
 assert(!api->start(api->context,&exposure) && exposure && allocCalls==2);
#endif
#if defined(A04_AUDIO_UNMUTE_DIAGNOSTIC) && A04_AUDIO_UNMUTE_DIAGNOSTIC
 assert(parent_unmutes==1);
#endif
 holdPosition=true;enteredPosition=false;revokeDone=false;
 std::thread positionReader([&]{KSAUDIO_POSITION p{};assert(api->position(api->context,&p)==STATUS_DEVICE_NOT_READY);});
 while(!enteredPosition)std::this_thread::yield();
 std::thread stopper([&]{assert(!api->stop(api->context));revokeDone=true;});
 while(!test_interrupt.fn)std::this_thread::yield();
 assert(!revokeDone);holdPosition=false;positionReader.join();stopper.join();assert(revokeDone);
 Mt8167PnpBackendRevoke(owner);Mt8167PnpBackendRelease(owner);
 api->dereference(api->context);
 assert(!freed && !puts && parent_refs==2 && test_pdo_refs==1);return 0;
#else
 assert(api->start(api->context,&exposure)==STATUS_DEVICE_NOT_READY && !exposure);
#endif
#if defined(A04_AUDIO_DIAGNOSTIC_TRACE) && A04_AUDIO_DIAGNOSTIC_TRACE && defined(A04_AUDIO_RUN_DIAGNOSTIC) && A04_AUDIO_RUN_DIAGNOSTIC
 assert(a04_test_trace_count>0);
#endif
 KSAUDIO_POSITION position{};assert(api->position(api->context,&position)==STATUS_DEVICE_NOT_READY);
 holdPosition=true;enteredPosition=false;revokeDone=false;
 std::thread reader([&] { KSAUDIO_POSITION p{};assert(api->position(api->context,&p)==STATUS_DEVICE_NOT_READY); });
 while(!enteredPosition)std::this_thread::yield();
 std::thread revoker([&] { Mt8167PnpBackendRevoke(owner);revokeDone=true; });
 while(Mt8167PnpBackendInterrupt(owner,&irq)!=STATUS_DEVICE_NOT_READY)std::this_thread::yield();
 assert(!revokeDone);holdPosition=false;reader.join();revoker.join();assert(revokeDone);
 assert(Mt8167PnpBackendAttachShared(owner,&shared)==STATUS_DEVICE_NOT_READY);assert(Mt8167PnpBackendInterrupt(owner,&irq)==STATUS_DEVICE_NOT_READY);Mt8167PnpBackendRelease(owner);
 assert(Mt8167PnpBackendBindInterrupt(owner,&transport)==STATUS_DEVICE_NOT_READY);
 assert(api->position(api->context,&position)==STATUS_DEVICE_NOT_READY);
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
 assert(maps==5 && unmaps==4 && test_irq_connections==1 && test_irq_disconnections==1 && parent_releases==1 && !test_interrupt.fn);
#else
 assert(maps==2 && unmaps==1);
#endif
#endif
 PMDL second=nullptr;assert(api->allocate(api->context,8192,&second,&n,&offset,&cache)==STATUS_DEVICE_NOT_READY && !second);
 assert(!freed && !puts && test_pdo_refs==1);
 api->freeBuffer(api->context,mdl,8192);assert(freed==1 && test_mdl_frees==1);
 api->dereference(api->context);assert(puts==1 && test_pdo_refs==0);
#if defined(A04_AUDIO_QUERY_SHARED) && A04_AUDIO_QUERY_SHARED
 assert(!parent_refs);
#endif
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
 assert(unmaps==4);
 /* IRQ disconnect precedes a failed release, retaining mapping and parent. */
 parent_prepare_fail=0;parent_release_fail=1;parent_releases=0;owner=nullptr;
 assert(!Mt8167PnpBackendCreate(&adapter,&resources,&owner) && owner && test_interrupt.fn);
 Mt8167PnpBackendRevoke(owner);Mt8167PnpBackendRelease(owner);
 assert(maps==6 && unmaps==4 && parent_refs==2 && test_pdo_refs==1 && test_irq_disconnections==2 && !test_interrupt.fn);
 /* A quarantined lease retains the binding; a second child is rejected. */
 parent_prepare_fail=1;parent_release_fail=1;owner=nullptr;
 assert(Mt8167PnpBackendCreate(&adapter,&resources,&owner)==STATUS_DEVICE_NOT_READY && !owner);
 assert(maps==7 && unmaps==5 && parent_refs==2 && test_pdo_refs==1 && test_irq_disconnections==2);
#else
 assert(unmaps==1);
#endif
#endif
}
