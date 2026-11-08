#include <cassert>
#include <cstring>
#include <cstdio>
#include "stream_transport.h"
#include "shared_transport.h"
int test_mdl_fail=0,test_mdl_frees=0,test_barriers=0,test_flushes=0;
_KINTERRUPT test_interrupt{}; PDMA_ADAPTER test_dma_adapter=nullptr;DEVICE_DESCRIPTION test_dma_description{};
static int step,failat,clock_held,irq_on,starts,stops,disconnects;
extern "C" int mt8167_clock_acquire(mt8167_clock_owner *,unsigned,uint32_t) {if(++step==failat)return -1;clock_held=1;return 0;}
extern "C" int mt8167_clock_release(mt8167_clock_owner *,unsigned) {clock_held=0;return 0;}
NTSTATUS Mt8167CommonPrepareForDevice(Mt8167CommonDma *b) {if(++step==failat)return -1;b->prepared=TRUE;return 0;}
NTSTATUS Mt8167CommonMarkExposed(Mt8167CommonDma *b) {if(++step==failat)return -1;b->exposed=TRUE;return 0;}
NTSTATUS Mt8167ConnectIrq(Mt8167Irq *i,mt8167_dl1_engine *e,ULONG,KIRQL,KAFFINITY,KINTERRUPT_MODE,MT8167_IRQ_NOTIFY,void *) {if(++step==failat)return -1;i->interrupt=&test_interrupt;i->engine=e;irq_on=1;return 0;}
void Mt8167DisconnectIrq(Mt8167Irq *i) {i->interrupt=nullptr;irq_on=0;disconnects++;}
int Mt8167SynchronizedStart(Mt8167Irq *irq,const struct mt8167_dl1_plan *plan) {starts++;if(++step==failat)return -1;
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
 return mt8167_dl1_start(irq->engine,plan);
#else
 return 0;
#endif
}
int Mt8167SynchronizedStop(Mt8167Irq *irq) {stops++;if(++step==failat)return -1;
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
 return mt8167_dl1_stop(irq->engine);
#else
 return 0;
#endif
}
int Mt8167SynchronizedPosition(Mt8167Irq *,const struct mt8167_dl1_plan *p,uint32_t *out) { assert(p->base==0x100000);*out=128;return 0; }
static void notify(void *,ULONG,BOOLEAN){}
static int parent_refs,parent_leases;
static int parent_ref(void *) { parent_refs++;return 0; }
static void parent_deref(void *) { parent_refs--; }
static int parent_acquire(void *,uint32_t rate) { assert(rate==48000);parent_leases++;return 0; }
static int parent_release(void *) { parent_leases--;return 0; }
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
static uint32_t registers[1024];static int addressWrites;
static int readreg(void*,uint32_t off,uint32_t*out){*out=registers[off/4];return 0;}
static int writereg(void*,uint32_t off,uint32_t value){if(off==0x40||off==0x48)addressWrites++;registers[off/4]=value;if(off==0x3a8)registers[0x3a4/4]&=~value;return 0;}
int Mt8167SynchronizedRestart(Mt8167Irq*irq,const struct mt8167_dl1_plan*p,BOOLEAN reset){return mt8167_dl1_restart(irq->engine,p,reset);}
int Mt8167DrainStoppedIrq(Mt8167Irq*irq){assert(irq->engine->state==MT8167_DL1_STOPPED);return 0;}
#endif
int main(){
#if defined(A04_AUDIO_HP_SHARED) && A04_AUDIO_HP_SHARED
 /* One real transport+engine execution; OS synchronization/MMIO are mocked.
  * Retained44100->48000 uses the same pages/SG/BASE/END, without a free. */
 Mt8167CommonDma dma{};_MDL mdl{};dma.mdl=&mdl;dma.size=49152;dma.logical.QuadPart=0x100000;
 mt8167_dl1_engine engine{{nullptr,readreg,writereg},MT8167_DL1_IDLE};Mt8167Irq irq{};
 irq.engine=&engine;irq.interrupt=&test_interrupt;irq.active=1;
 Mt8167Transport t{};t.dma=&dma;t.irq=&irq;t.engine=&engine;
 mt8167_audio_shared_client shared{};shared.referenced=1;shared.leased=1;
 assert(!Mt8167TransportStartHpCalibration(&t,24576,&shared)&&t.plan.rate_code==9&&t.state==AUDIO_RUNNING&&dma.exposed&&shared.quarantined);
 assert(addressWrites==2&&registers[0x40/4]==0x100000&&registers[0x48/4]==0x10bfff);
 assert(Mt8167TransportHpRetainedPcmPlan(&t)==-1);
 assert(!Mt8167TransportStop(&t)&&t.state==AUDIO_STOPPED_RETAINED);
 assert(!Mt8167TransportHpRetainedPcmPlan(&t)&&t.plan.rate_code==10);
 assert(!Mt8167TransportRestartRetained(&t,FALSE)&&t.state==AUDIO_RUNNING&&addressWrites==2);
 assert((registers[0x14/4]&15)==10&&dma.mdl==&mdl&&dma.exposed&&irq.interrupt&&!disconnects&&shared.quarantined);
 assert(!Mt8167TransportStop(&t)&&t.state==AUDIO_STOPPED_RETAINED);
 registers[0x48/4]++;assert(Mt8167TransportRestartRetained(&t,FALSE)==-1&&t.state==AUDIO_QUARANTINED&&addressWrites==2&&dma.exposed);
 puts("HP real transport+DL1 engine44100->stopped->48000 same-ring restart: address writes only initial2; changed-END rejected; pinned ring/IRQ retained PASS (OS/MMIO mock)");
#else
 for(int fail=1;fail<=5;fail++) {
  Mt8167CommonDma dma{};_MDL mdl{};dma.mdl=&mdl;dma.size=8192;dma.logical.QuadPart=0x100000;
  Mt8167Irq irq{};mt8167_dl1_engine engine{};mt8167_clock_owner clock{};
  Mt8167Transport t{};t.dma=&dma;t.irq=&irq;t.engine=&engine;t.clock=&clock;t.clockRate=12288000;t.notify=notify;t.affinity=1;
  step=0;failat=fail;clock_held=irq_on=starts=stops=disconnects=0;
  assert(Mt8167TransportStart(&t,256)==-1);
  if(fail<5) assert(t.state==AUDIO_FAILED && !clock_held && !irq_on && !dma.exposed);
  else assert(t.state==AUDIO_QUARANTINED && clock_held && irq_on && dma.exposed);
 }
 Mt8167CommonDma dma{};_MDL mdl{};dma.mdl=&mdl;dma.size=8192;dma.logical.QuadPart=0x100000;
 Mt8167Irq irq{};mt8167_dl1_engine engine{};mt8167_clock_owner clock{};
 Mt8167Transport t{};t.dma=&dma;t.irq=&irq;t.engine=&engine;t.clock=&clock;t.clockRate=12288000;t.notify=notify;t.affinity=1;
 step=0;failat=0;clock_held=irq_on=starts=stops=disconnects=0;
 assert(!Mt8167TransportStart(&t,256) && t.state==AUDIO_RUNNING && dma.exposed && irq_on && clock_held);
 uint32_t position=0;assert(!Mt8167TransportPosition(&t,&position) && position==128);
 assert(!Mt8167TransportStop(&t) && t.state==AUDIO_QUARANTINED && dma.exposed && clock_held && !irq_on && disconnects==1);
 assert(Mt8167TransportPosition(&t,&position)==-1);
 assert(Mt8167TransportStart(&t,256)==-1);
 Mt8167CommonDma dma2{};_MDL mdl2{};dma2.mdl=&mdl2;dma2.size=8192;dma2.logical.QuadPart=0x200000;
 Mt8167Irq irq2{};Mt8167Transport t2{};t2.dma=&dma2;t2.irq=&irq2;t2.engine=&engine;t2.clock=&clock;t2.clockRate=12288000;t2.notify=notify;t2.affinity=1;
 step=0;failat=0;assert(!Mt8167TransportStart(&t2,256));failat=step+1;
 assert(Mt8167TransportStop(&t2)==-1 && t2.state==AUDIO_QUARANTINED && irq_on && clock_held && dma2.exposed);
 for(int fail=1;fail<=4;fail++) {
  Mt8167CommonDma d{};_MDL m{};d.mdl=&m;d.size=8192;d.logical.QuadPart=0x100000;
  Mt8167Irq i{};Mt8167Transport p{};p.dma=&d;p.irq=&i;p.engine=&engine;p.notify=notify;p.affinity=1;
  mt8167_audio_shared_client shared{};shared.referenced=1;shared.leased=1;
  step=0;failat=fail;clock_held=irq_on=0;
  assert(Mt8167TransportStartParentOwned(&p,256,&shared)==-1 && !clock_held);
  assert(shared.quarantined==(fail>1));
  if(fail<4) assert(p.state==AUDIO_FAILED && !irq_on && !d.exposed);
  else assert(p.state==AUDIO_QUARANTINED && irq_on && d.exposed);
 }
 /* Exercise parent lease, DMA preparation and IRQ connection together. */
 for(int fail=1;fail<=4;fail++) {
  Mt8167CommonDma d{};_MDL m{};d.mdl=&m;d.size=8192;d.logical.QuadPart=0x100000;
  Mt8167Irq i{};Mt8167Transport p{};p.dma=&d;p.irq=&i;p.engine=&engine;p.notify=notify;p.affinity=1;
  int context=0;mt8167_audio_shared service{sizeof(service),1,&context,parent_ref,parent_deref,parent_acquire,parent_release};
  mt8167_audio_shared_client shared{};parent_refs=parent_leases=0;assert(!mt8167_audio_shared_attach(&shared,&service));
  step=0;failat=fail;clock_held=irq_on=0;
  assert(Mt8167SharedTransportStart(&shared,&p,256)==-1 && !clock_held);
  if(fail==1) { assert(!shared.quarantined && !parent_leases && !irq_on && !d.exposed);assert(!mt8167_audio_shared_detach(&shared) && !parent_refs); }
  else { assert(shared.quarantined && parent_leases==1 && parent_refs==1);assert(mt8167_audio_shared_detach(&shared)==-1); }
 }
 /* PnP already connected the masked interrupt: do not connect twice or
  * release the lease on a failure after the exposure boundary. */
 for(int fail=0;fail<=3;fail++) {
  Mt8167CommonDma d{};_MDL m{};d.mdl=&m;d.size=8192;d.logical.QuadPart=0x100000;
  mt8167_dl1_engine e{};Mt8167Irq i{};i.engine=&e;i.interrupt=&test_interrupt;i.active=1;
  Mt8167Transport p{};p.dma=&d;p.irq=&i;p.engine=&e;
  mt8167_audio_shared_client shared{};shared.referenced=1;shared.leased=1;
  step=0;failat=fail;irq_on=1;disconnects=0;starts=0;
  int result=Mt8167TransportStartMaskedParent(&p,256,&shared);
  if(!fail) {assert(!result && p.state==AUDIO_RUNNING && shared.quarantined && d.exposed && starts==1);}
  else {assert(result && i.interrupt && !disconnects);assert(shared.quarantined==(fail>1));assert(d.exposed==(fail==3));}
 }

#endif
}
