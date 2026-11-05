#include <assert.h>
#include "audio_parent_client.h"
static int refs=1,attempts;
static VOID ref(PVOID p){(void)p;refs++;}static VOID deref(PVOID p){(void)p;refs--;}
static int acquire(PVOID p,ULONG rate){(void)p;assert(rate==48000);attempts++;return -1;}
static int release(PVOID p){(void)p;return -1;}
static int prepare(PVOID p,PVOID a,ULONG n){(void)p;(void)a;(void)n;return -1;}
static int bind(PVOID p,PVOID a,ULONG n){(void)p;(void)a;(void)n;return 0;}
int main(void){int obj;struct Mt8167AudioParentClient c={0};struct mt8167_audio_shared_client client={0};A04_SHARED_AUDIO_INTERFACE p={0};
 p.Header.Size=sizeof(p);p.Header.Version=A04_SHARED_AUDIO_INTERFACE_VERSION;p.Header.Context=&obj;p.Header.InterfaceReference=ref;p.Header.InterfaceDereference=deref;p.AcquireMuted=acquire;p.ReleaseQuiescent=release;p.PrepareMaskedIrq=prepare;p.BindAfe=bind;
 p.Header.Version=1;assert(Mt8167AudioParentBind(&c,&p)==-1 && refs==1);p.Header.Version=A04_SHARED_AUDIO_INTERFACE_VERSION;
 assert(!Mt8167AudioParentBind(&c,&p));assert(!mt8167_audio_shared_attach(&client,Mt8167AudioParentService(&c)) && refs==2);
 assert(Mt8167AudioParentClose(&c)==-1 && refs==2);assert(mt8167_audio_shared_acquire(&client,48000)==-1 && attempts==1);
 assert(!mt8167_audio_shared_detach(&client) && refs==1);assert(!Mt8167AudioParentClose(&c) && !refs);
}
