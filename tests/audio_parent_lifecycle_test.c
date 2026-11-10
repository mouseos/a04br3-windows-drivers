/* Execute the real parent lifecycle implementation with explicit PMIC/clock
 * providers. This is protocol validation, not physical bus-idle evidence. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "audio_query_stubs/ntddk.h"
#define A04_SHARED_AUDIO_LIFECYCLE 1
#define A04_SHARED_ENGINE_PCM 1
#define A04_SHARED_FIRST_PLAYBACK 1
#define A04_SHARED_DIGITAL_PLAYBACK 1
#define A04_SHARED_VOICE_PCM_DIAGNOSTIC 1
typedef PVOID WDFDEVICE;
#define MAXULONG UINT32_MAX
#define STATUS_INTEGER_OVERFLOW (-22)
#include "../drivers/bus/mt8167-shared/digital_playback.h"
#include "../drivers/bus/mt8167-shared/audio_lifecycle_interface.h"
struct fake_owner {int leased,poisoned;};
struct fake_clock {int held,poisoned;};
struct fake_voice {int active;};struct a04_codec_voice_io {int unused;};
struct fake_classd {int muted;};
typedef struct {
 void *audioAfe;struct fake_owner audio;struct {int online;}pmic;
 int afeClockHeld,afeClockPoisoned;struct fake_clock audioTopClock;
 ULONG audioGeneration,audioActiveGeneration,audioStopCertified,audioSavedMcuEn;int audioReleaseAuthorized,audioPowerPinned,audioMcuLeaseActive;
 struct fake_voice voice;struct fake_classd classd;struct a04_digital_playback digitalPlayback;
 uint32_t reg[1024];int locked,resumed,freed,failWrite;
} A04_PARENT_CONTEXT;
static A04_PARENT_CONTEXT *ParentContext(PVOID p){return p;}
static int ParentLock(A04_PARENT_CONTEXT*c){assert(!c->locked);c->locked=1;return 0;}
static void ParentUnlock(A04_PARENT_CONTEXT*c){assert(c->locked);c->locked=0;}
int KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
static int ParentDigitalRead(void*v,unsigned off,uint32_t*out){A04_PARENT_CONTEXT*c=v;*out=c->reg[off/4];return 0;}
static int ParentDigitalWrite(void*v,unsigned off,uint32_t x){A04_PARENT_CONTEXT*c=v;if(c->failWrite)return -1;c->reg[off/4]=x;return 0;}
static int ParentDigitalDelay(void*v,unsigned us){(void)v;assert(us==100);return 0;}
static int ParentAudioAcquire(PVOID p,ULONG rate){A04_PARENT_CONTEXT*c=p;assert(rate==48000&&!c->audio.leased);c->audio.leased=1;c->audioPowerPinned=1;c->afeClockHeld=1;c->audioTopClock.held=1;c->classd.muted=1;return 0;}
static int a04_mt6392_classd_verify_muted(struct fake_classd*c){return !c->muted;}
static struct a04_codec_voice_io ParentVoiceIo(A04_PARENT_CONTEXT*c){(void)c;struct a04_codec_voice_io io={0};return io;}
static int a04_codec_voice_stop(struct fake_voice*v,struct a04_codec_voice_io*io){(void)io;v->active=0;return 0;}
static A04_PARENT_CONTEXT *active;
static int a04_audio_owner_release(struct fake_owner*o){
 assert(active->audioReleaseAuthorized && !active->voice.active && !active->digitalPlayback.retained);
 assert(!(active->reg[0x10/4]&1u) && !(active->reg[0x34/4]&1u));
 o->leased=0;active->afeClockHeld=0;active->audioTopClock.held=0;return 0;
}
static void WdfDeviceResumeIdle(PVOID p){A04_PARENT_CONTEXT*c=p;assert(!c->locked && !c->audio.leased);c->resumed++;}
#include "../drivers/bus/mt8167-shared/audio_lifecycle_parent.inc"
static void initialize(A04_PARENT_CONTEXT*c){memset(c,0,sizeof(*c));c->pmic.online=1;c->audioAfe=c;c->reg[0]=(1u<<25)|(1u<<26);active=c;}
int main(void){
 A04_PARENT_CONTEXT c;initialize(&c);ULONG generation=0;
 struct a04_digital_io io={&c,ParentDigitalRead,ParentDigitalWrite,ParentDigitalDelay};
 assert(!ParentLifecycleAcquire(&c,48000,&generation) && generation==1);
 c.audioSavedMcuEn=0x80000055u;c.audioMcuLeaseActive=1;c.reg[0x3b4/4]=0x800000ffu;
 assert(!a04_digital_prepare(&c.digitalPlayback,&io,48000));c.voice.active=1;
 c.reg[0x10/4]|=2u;c.reg[0x3a0/4]|=1u;c.reg[0x3a4/4]=1u;
 assert(ParentLifecycleCertify(&c,generation,1)<0 && !c.audioStopCertified);
 c.reg[0x10/4]&=~2u;c.reg[0x3a0/4]&=~1u;c.reg[0x3a4/4]=0;
 assert(ParentLifecycleCertify(&c,generation,0)<0); // no IRQ rundown receipt
 assert(!ParentLifecycleCertify(&c,generation,1) && c.audioPowerPinned && !c.resumed);
 assert(ParentLifecycleRelease(&c,generation+1)<0 && c.audio.leased);
 assert(!ParentLifecycleRelease(&c,generation) && !c.audio.leased && c.resumed==1 && !c.audioPowerPinned);
 assert(!c.digitalPlayback.prepared && c.reg[0]==((1u<<25)|(1u<<26)));
 assert(c.reg[0x3b4/4]==0x80000055u && !c.audioMcuLeaseActive && !c.audioActiveGeneration);
 assert(!ParentLifecycleAcquire(&c,48000,&generation) && generation==2);
 assert(ParentLifecycleRelease(&c,1)<0); // stale stop never authorizes the new lease
 assert(!a04_digital_prepare(&c.digitalPlayback,&io,48000));c.voice.active=1;
 assert(!ParentLifecycleCertify(&c,generation,1));c.failWrite=1;
 assert(ParentLifecycleRelease(&c,generation)<0 && c.audio.poisoned && c.audio.leased && c.audioPowerPinned && c.resumed==1);
 initialize(&c);c.audioGeneration=MAXULONG;
 assert(ParentLifecycleAcquire(&c,48000,&generation)==STATUS_INTEGER_OVERFLOW && !c.audio.leased && !generation);
 return 0;
}
