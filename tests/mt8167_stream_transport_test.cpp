#include <cassert>
#include <cstring>
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
int Mt8167SynchronizedStart(Mt8167Irq *,const struct mt8167_dl1_plan *) {starts++;if(++step==failat)return -1;return 0;}
int Mt8167SynchronizedStop(Mt8167Irq *) {stops++;if(++step==failat)return -1;return 0;}
int Mt8167SynchronizedPosition(Mt8167Irq *,const struct mt8167_dl1_plan *p,uint32_t *out) { assert(p->base==0x100000);*out=128;return 0; }
static void notify(void *,ULONG,BOOLEAN){}
static int parent_refs,parent_leases;
static int parent_ref(void *) { parent_refs++;return 0; }
static void parent_deref(void *) { parent_refs--; }
static int parent_acquire(void *,uint32_t rate) { assert(rate==48000);parent_leases++;return 0; }
static int parent_release(void *) { parent_leases--;return 0; }
int main(){
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

}
