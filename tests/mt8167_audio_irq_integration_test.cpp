#include <cassert>
#include <cstring>
#include "shared_transport.h"
#if defined(A04_AUDIO_DIAGNOSTIC_TRACE) && A04_AUDIO_DIAGNOSTIC_TRACE
int a04_test_trace_count=0;
#endif
int test_mdl_fail=0,test_mdl_frees=0,test_barriers=0,test_flushes=0;
_KINTERRUPT test_interrupt{};PDMA_ADAPTER test_dma_adapter=nullptr;DEVICE_DESCRIPTION test_dma_description{};
static uint32_t regs[0x400/4];static int refs,leases,notified,faulted;
static bool failEnable,failStop;
extern "C" int mt8167_clock_acquire(mt8167_clock_owner *,unsigned,uint32_t){return -1;}
extern "C" int mt8167_clock_release(mt8167_clock_owner *,unsigned){return -1;}
NTSTATUS Mt8167CommonPrepareForDevice(Mt8167CommonDma *d){d->prepared=TRUE;return STATUS_SUCCESS;}
NTSTATUS Mt8167CommonMarkExposed(Mt8167CommonDma *d){d->exposed=TRUE;return STATUS_SUCCESS;}
static int read_reg(void *,uint32_t off,uint32_t *value){assert(!(off&3) && off<0x400);*value=regs[off/4];return 0;}
static int write_reg(void *,uint32_t off,uint32_t value){assert(!(off&3) && off<0x400);regs[off/4]=value;if(off==0x3a8)regs[0x3a4/4]&=~value;
 if(failEnable && off==0x3a0 && (value&1u)){failEnable=false;regs[0x3a4/4]|=1;return -1;}
 if(failStop && off==0x10 && !(value&2u)){failStop=false;return -1;}
 return 0;}
static int ref(void *){refs++;return 0;}static void deref(void *){refs--;}
static int acquire(void *,uint32_t rate){assert(rate==48000);leases++;return 0;}static int release(void *){leases--;return 0;}
static void notify(void *,ULONG count,BOOLEAN fault){notified+=(int)count;faulted=fault;}
int main(){
 int context=0;mt8167_audio_shared s{sizeof(s),1,&context,ref,deref,acquire,release};mt8167_audio_shared_client shared{};assert(!mt8167_audio_shared_attach(&shared,&s));
 _MDL mdl{};Mt8167CommonDma dma{};dma.mdl=&mdl;dma.size=8192;dma.logical.QuadPart=0x100000;
 mt8167_dl1_engine engine{};engine.io.read32=read_reg;engine.io.write32=write_reg;
 Mt8167Irq irq{};Mt8167Transport t{};t.dma=&dma;t.irq=&irq;t.engine=&engine;t.vector=80;t.level=5;t.affinity=1;t.mode=LevelSensitive;t.notify=notify;
 assert(!Mt8167SharedTransportStart(&shared,&t,256) && dma.exposed && shared.quarantined && irq.interrupt);
 assert(regs[0x40/4]==0x100000 && regs[0x48/4]==0x101fff && (regs[0x10/4]&2));
 regs[0x44/4]=0x100100;regs[0x3a4/4]=1;assert(test_interrupt.fn(&test_interrupt,test_interrupt.ctx));assert(!regs[0x3a4/4] && irq.dpc.queued);
 irq.dpc.queued=0;irq.dpc.fn(&irq.dpc,irq.dpc.ctx,(PVOID)(ULONG_PTR)irq.generation,nullptr);assert(notified==1 && !faulted);
 uint32_t pos=0;assert(!Mt8167TransportPosition(&t,&pos) && pos==256);
 assert(!Mt8167TransportStop(&t) && !irq.interrupt && !(regs[0x10/4]&2));
#if defined(A04_AUDIO_DIAGNOSTIC_TRACE) && A04_AUDIO_DIAGNOSTIC_TRACE
 assert(a04_test_trace_count>0);
#endif
 assert(Mt8167TransportPosition(&t,&pos)==-1 && dma.exposed && shared.quarantined && refs==1 && leases==1);
 // Test synchronized failure containment against ambiguous writes that really
 // changed mock hardware. This does not retry a poisoned production owner.
 struct mt8167_dl1_plan plan{};assert(mt8167_dl1_plan(0x100000,8192,4096,48000,&plan));
 for(int fault=0;fault<2;fault++) {
  std::memset(regs,0,sizeof(regs));regs[0x3a0/4]=0x100;
  mt8167_dl1_engine failedEngine{};failedEngine.io.read32=read_reg;failedEngine.io.write32=write_reg;
  Mt8167Irq failedIrq{};assert(!Mt8167ConnectIrq(&failedIrq,&failedEngine,80,5,1,LevelSensitive,notify,nullptr));
  if(!fault) {
   failEnable=true;assert(Mt8167SynchronizedStart(&failedIrq,&plan)<0);
   assert(regs[0x10/4]&2u); // not a DMA-stop/drain certificate
  } else {
   assert(!Mt8167SynchronizedStart(&failedIrq,&plan));
   regs[0x3a4/4]=1;failStop=true;assert(Mt8167SynchronizedStop(&failedIrq)<0);
  }
  assert(failedIrq.fault && !failedIrq.armed && failedEngine.state==MT8167_DL1_POISONED);
  assert(regs[0x3a0/4]==0x1a0 && !(regs[0x3a4/4]&1u));
  assert(dma.exposed && shared.quarantined && refs==1 && leases==1);
  Mt8167DisconnectIrq(&failedIrq);
 }

 assert(mt8167_audio_shared_detach(&shared)==-1);
}
