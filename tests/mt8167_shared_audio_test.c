#include <assert.h>
#include "shared_audio.h"
struct mock {int refs,leases,fail_ref,fail_acquire,fail_release;};
static int ref(void *v){struct mock*m=v;if(m->fail_ref)return -1;m->refs++;return 0;}
static void deref(void*v){((struct mock*)v)->refs--;}
static int acquire(void*v,uint32_t rate){struct mock*m=v;assert(rate==48000);if(m->fail_acquire)return -m->fail_acquire;m->leases++;return 0;}
static int release(void*v){struct mock*m=v;if(m->fail_release)return -1;m->leases--;return 0;}
int main(void){struct mock m={0};struct mt8167_audio_shared_client c={0};struct mt8167_audio_shared s={sizeof(s),1,&m,ref,deref,acquire,release};
 s.version=2;assert(mt8167_audio_shared_attach(&c,&s)==-1 && !m.refs);s.version=1;
 m.fail_ref=1;assert(mt8167_audio_shared_attach(&c,&s)==-1 && !m.refs);m.fail_ref=0;
 assert(!mt8167_audio_shared_attach(&c,&s) && m.refs==1);m.fail_acquire=1;assert(mt8167_audio_shared_acquire(&c,48000)==-1 && !m.leases);m.fail_acquire=0;
 assert(!mt8167_audio_shared_acquire(&c,48000) && m.leases==1);assert(mt8167_audio_shared_detach(&c)==-1);
 assert(!mt8167_audio_shared_release_unexposed(&c) && !m.leases);assert(!mt8167_audio_shared_detach(&c) && !m.refs);
 {struct mt8167_audio_shared_client ambiguous={0};m.fail_acquire=2;
  assert(!mt8167_audio_shared_attach(&ambiguous,&s));assert(mt8167_audio_shared_acquire(&ambiguous,48000)==-1);
  assert(ambiguous.leased && ambiguous.quarantined && mt8167_audio_shared_detach(&ambiguous)==-1 && m.refs==1);
  /* Mock-only disposal; production must retain this parent reference. */
  m.refs=0;m.fail_acquire=0;
 }
 assert(!mt8167_audio_shared_attach(&c,&s));assert(!mt8167_audio_shared_acquire(&c,48000));assert(!mt8167_audio_shared_quarantine(&c));
 assert(mt8167_audio_shared_release_unexposed(&c)==-1 && mt8167_audio_shared_detach(&c)==-1 && m.refs==1 && m.leases==1);
}
