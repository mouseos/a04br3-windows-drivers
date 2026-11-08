"""Compile actual backend functions with mocked OS/DMA; new HP boundaries only."""
from pathlib import Path
import argparse, os, subprocess
p=argparse.ArgumentParser();p.add_argument('--scratch',type=Path,required=True);a=p.parse_args()
repo=Path(__file__).resolve().parents[1];scratch=a.scratch.resolve();assert scratch.is_dir()
s=(repo/'drivers/audio/mt8167-afe/pnp_backend.cpp').read_text()
def function(name):
    # Identify definition directly, excluding forward declarations.
    import re
    m=re.search(r'^static [^\n]*\b'+name+r'\([^;]*?\)\s*\{',s,re.M);assert m,name
    begin=m.start();end=m.end();depth=1
    while depth:
        if s[end]=='{':depth+=1
        if s[end]=='}':depth-=1
        end+=1
    return s[begin:end]
code=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
using NTSTATUS=int;using ULONG=unsigned;using LONG=int;using BOOLEAN=bool;
using PVOID=void*;using PPORTWAVERTSTREAM=void*;using PMDL=void*;using PULONG=unsigned*;
#define TRUE true
#define FALSE false
#define PASSIVE_LEVEL 0
#define STATUS_SUCCESS 0
#define STATUS_DEVICE_NOT_READY -1
#define STATUS_DEVICE_BUSY -2
#define STATUS_INVALID_PARAMETER -3
#define STATUS_IO_TIMEOUT -4
#define STATUS_BUFFER_TOO_SMALL -5
#define STATUS_DEVICE_CONFIGURATION_ERROR -6
#define NT_SUCCESS(s) ((s)>=0)
#define A04_HP_DRAIN_UNKNOWN 0
#define A04_HP_DRAIN_RETAINED 1
#define A04_HP_DRAIN_RELEASED 2
#define A04_STAGE_STOP 1
#define A04_STAGE_STOPPED_PINNED 2
#define A04_TRACE(...) ((void)0)
#define A04_AUDIO_HP_SHARED 1
#define A04_AUDIO_ENGINE_PCM 1
#define A04_AUDIO_RUN_DIAGNOSTIC 1
#define A04_AUDIO_VOICE_PCM_DIAGNOSTIC 1
static int sideEffects,locks;
static int HpCalibrateLocked(struct Mt8167PnpBackend*);static std::vector<int> events;static unsigned drainState=1;static int hpError;
enum{AUDIO_READY,AUDIO_RUNNING,AUDIO_STOPPED_RETAINED};enum{KSSTATE_STOP};
struct Buffer{PMDL mdl;void*mapping;unsigned bytes;uint64_t physicalAddress;};
struct Transport{int state;int plan;};
struct Parent{struct{void*Context;}Header;int(*StartVoice)(void*);int(*Mute)(void*);int(*UnmuteMin)(void*);int(*StopVoice)(void*);};
struct Hp{struct{void*Context;}Header;int(*Start)(void*);int(*MuteRetained)(void*);int(*ResumeRetained)(void*);int(*Stop)(void*,unsigned*);};
struct Mt8167PnpBackend{
 bool digitalRetained,hpPinned,hpSelected,hpAnalogStarted,online,waveProbePending,livePositionsClosed;
 LONG enginePoison,stoppedOffset,positionPaused;PPORTWAVERTSTREAM engineClient;
 Transport transport;Buffer retainedBuffer;int engineState;bool resetOnRun;
 struct{Parent parent;}parentClient;Hp hpParent;struct{unsigned stoppedCurrent;}irq;
 int livePositions,diagStage,diagStatus;
};
static void OwnerLock(Mt8167PnpBackend*){assert(!locks++);}static void OwnerUnlock(Mt8167PnpBackend*){assert(--locks==0);}
static int KeGetCurrentIrql(){return 0;}
static int Mt8167AllocateBuffer(void*,unsigned,Buffer*){sideEffects++;return -1;}
static int OwnerBindWaveLocked(Mt8167PnpBackend*,PMDL,void*,unsigned){sideEffects++;return -1;}
static int Mt8167ReleaseUnpublishedBuffer(Buffer*){sideEffects++;return 0;}
static int Mt8167VerifyWaveCache(void*,unsigned,uint64_t){sideEffects++;return 0;}
static int InterlockedExchange(int*p,int v){int old=*p;*p=v;return old;}
#define RtlZeroMemory(p,n) (sideEffects++,memset(p,0,n))
static void ExWaitForRundownProtectionRelease(int*){events.push_back(10);}
static int Mt8167TransportStop(Transport*t){events.push_back(30);t->state=AUDIO_STOPPED_RETAINED;return 0;}
static int mt8167_dl1_position(int*,unsigned,unsigned*){return 1;}
static int hstart(void*){events.push_back(1);return 0;}
static int hmute(void*){events.push_back(20);return 0;}
static int hresume(void*){events.push_back(2);return 0;}
static int hstop(void*,unsigned*s){events.push_back(40);*s=drainState;return hpError;}
static int speaker(void*){events.push_back(99);return 0;}
'''
for name in ['OwnerCheckoutRoute','OwnerRouteStart','OwnerRouteMute','OwnerRouteUnmute','OwnerStopRunning']:
    code+='\n'+function(name)+'\n'
code+=r'''
static int HpCalibrateLocked(Mt8167PnpBackend*){return STATUS_DEVICE_NOT_READY;}
int main(){
 char backing[8192];Mt8167PnpBackend o{};o.online=true;o.transport.state=AUDIO_STOPPED_RETAINED;
 o.retainedBuffer={(void*)1,backing,sizeof(backing),0x100000};
 o.parentClient.parent={ {nullptr},speaker,speaker,speaker,speaker};
 o.hpParent={{nullptr},hstart,hmute,hresume,hstop};PMDL mdl;unsigned actual;
 for(int prior=0;prior<2;prior++){
  o.digitalRetained=true;o.hpPinned=prior;o.hpSelected=prior;
  assert(OwnerCheckoutRoute(&o,(void*)2,8192,&mdl,&actual,!prior)==STATUS_DEVICE_BUSY);
  assert(!sideEffects&&!mdl&&!actual&&o.hpSelected==bool(prior));
 }
 o.hpSelected=true;o.hpAnalogStarted=false;assert(OwnerRouteStart(&o)==0&&o.hpPinned&&o.hpAnalogStarted);
 assert(OwnerRouteUnmute(&o)==0);assert((events==std::vector<int>{1,2}));events.clear();
 o.transport.state=AUDIO_RUNNING;
 assert(OwnerStopRunning(&o)==0&&o.transport.state==AUDIO_STOPPED_RETAINED&&o.hpAnalogStarted);
 assert((events==std::vector<int>{10,20,30,40}));assert(o.retainedBuffer.mdl==(void*)1&&o.hpPinned);events.clear();
 assert(OwnerRouteStart(&o)==0&&events.empty());assert(OwnerRouteUnmute(&o)==0);events.clear();
 drainState=0;o.livePositionsClosed=false;o.transport.state=AUDIO_RUNNING;
 assert(OwnerStopRunning(&o)==STATUS_DEVICE_NOT_READY&&o.enginePoison&&o.retainedBuffer.mdl==(void*)1&&o.hpPinned);
 assert((events==std::vector<int>{10,20,30,40}));
}
'''
source=scratch/'backend-boundaries.cpp';source.write_text(code)
exe=scratch/'backend-boundaries';env=dict(os.environ,TMPDIR=str(scratch))
subprocess.run(['c++','-std=c++14','-O1','-g','-fsanitize=address,undefined',str(source),'-o',str(exe)],check=True,env=env)
subprocess.run([str(exe)],check=True,env=env)
print('HP actual backend functions: retained route exclusion before side-effects; start/resume; rundown->clamp->DL1 stop->typed drain; unknown drain retains/poisons PASS (mock only)')
