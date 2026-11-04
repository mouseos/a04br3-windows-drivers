#include <cassert>
#include "shared_transport.h"
int test_mdl_fail=0,test_mdl_frees=0,test_barriers=0,test_flushes=0;_KINTERRUPT test_interrupt{};PDMA_ADAPTER test_dma_adapter=nullptr;DEVICE_DESCRIPTION test_dma_description{};
static int refs,leases,calls,fail;
static int ref(void *){refs++;return 0;}static void deref(void *){refs--;}
static int acquire(void *,uint32_t r){assert(r==48000);if(fail)return -1;leases++;return 0;}static int release(void *){leases--;return 0;}
int Mt8167TransportStart(Mt8167Transport *,uint32_t){calls++;return -1;}
static void notify(void *,ULONG,BOOLEAN){}
int main(){
 mt8167_audio_shared_client c{};int ctx;mt8167_audio_shared s{sizeof(s),1,&ctx,ref,deref,acquire,release};
 Mt8167Transport t{};Mt8167CommonDma dma{};Mt8167Irq irq{};mt8167_dl1_engine eng{};mt8167_clock_owner clock{};
 t.dma=&dma;t.irq=&irq;t.engine=&eng;t.clock=&clock;t.notify=notify;t.affinity=1;
 assert(Mt8167SharedTransportStart(&c,&t,256)==-1 && !calls);
 assert(!mt8167_audio_shared_attach(&c,&s));fail=1;assert(Mt8167SharedTransportStart(&c,&t,256)==-1 && !calls && !leases);
 fail=0;assert(Mt8167SharedTransportStart(&c,&t,256)==-1 && calls==1 && c.quarantined && leases==1 && refs==1);
 assert(mt8167_audio_shared_detach(&c)==-1 && mt8167_audio_shared_release_unexposed(&c)==-1);
}
