#!/usr/bin/env python3
"""Run the actual live-frame submission and retained CPU handshake on host."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1];g=root/'drivers/gpu/mt8167'
def fn(source,name):
 a=source.index(name);b=source.index('{',a);e=b+1;n=1
 while n:n+=(source[e]=='{')-(source[e]=='}');e+=1
 return source[a:e]
s=(g/'wddm-display/ge8300_display_resources.cxx').read_text()
functions=fn(s,'NTSTATUS A04DisplayResources::RetireToCpu(')+'\n'+fn(s,'NTSTATUS A04DisplayResources::SubmitLiveFrame(')
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <climits>
#include "ge8300_status.h"
#include "present_trace_policy.h"
#include "idle_transition_diagnostic.h"
using NTSTATUS=int;using BOOLEAN=int;using UCHAR=unsigned char;using ULONG=uint32_t;using ULONGLONG=uint64_t;
#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_NOT_SUPPORTED (-1)
#define STATUS_DEVICE_NOT_READY (-2)
#define STATUS_DATA_ERROR (-3)
#define STATUS_IO_DEVICE_ERROR (-4)
#define STATUS_INVALID_DEVICE_STATE (-5)
#define STATUS_INSUFFICIENT_RESOURCES (-6)
#define NT_SUCCESS(s) ((s)>=0)
#define MAXULONG UINT32_MAX
#define PASSIVE_LEVEL 0
#define NonPagedPoolNx 0
static int scenario,calls,retire_calls,allocs,proof_marks;
static int KeGetCurrentIrql(){return 0;}static int KeAreAllApcsDisabled(){return 0;}
static void *ExAllocatePoolWithTag(int,size_t n,int){++allocs;return malloc(n);}
static void ExFreePoolWithTag(void *p,int){--allocs;free(p);}
static ULONGLONG KeQueryInterruptTime(){return 0;}
static void A04DisplayTrace(int stage,NTSTATUS){if(stage==0xc4)++proof_marks;}
#define RtlCopyMemory std::memcpy
static void KeFlushIoBuffers(void*,int,int){}static void KeMemoryBarrier(){}
struct Ops {int (*FlushAdapterBuffersEx)(void*,void*,void*,int,int,int);};
struct Adapter {Ops *DmaOperations;};
static int flush(void*,void*,void*,int,int,int){return scenario==4?-4:0;}
struct A04CommonFrameOwner {
 int sealed;void *cpu;struct {void *mdl;Adapter *gpu_adapter;} shared;void *mapRegisterBase;
 uint32_t frame,task,flush,cpuMatches;
};
struct A04DisplayResources {
 void *SharedCommonOwner,*GpuHandle;struct {uint32_t token;} Lease;
 int DirectPostActive,LiveReady,LiveFaulted,LiveShadowValid,LiveCpuFallback,IdleRetirementMarked;uint32_t LiveFrames;
 NTSTATUS RetireToCpu();NTSTATUS SubmitLiveFrame(const void*,uint32_t);
};
static NTSTATUS LiveFrameIoctl(void*,uint32_t control,void *data,size_t,size_t) {
 ++calls;
 if(control==A04_GE8300_IOCTL_RETAINED_CPU) {
  ++retire_calls;auto *p=(a04_ge8300_retained_cpu*)data;
  assert(p->version==1 && p->bytes==16 && p->token==17 && !p->flags);
  if(scenario==2)return STATUS_IO_DEVICE_ERROR;
  p->flags=scenario==5?1:3;return 0;
 }
 if(scenario==1 || scenario==2 || scenario==5)return STATUS_DEVICE_NOT_READY;
 if(control==A04_GE8300_IOCTL_SHARED_WIDE_FRAME) {
  if(scenario==3)return STATUS_IO_DEVICE_ERROR;
  auto *p=(a04_ge8300_shared_wide_frame*)data;p->task=5;p->flush=7;
 }
 return 0;
}
'''
main=r'''
int main(){
 Ops ops={flush};Adapter adapter={&ops};
 for(int i=0;i<=5;i++){
  scenario=i;calls=retire_calls=allocs=proof_marks=0;
  A04CommonFrameOwner owner={};owner.sealed=1;owner.cpu=calloc(1,4096000);
  owner.shared.mdl=&owner;owner.shared.gpu_adapter=&adapter;owner.mapRegisterBase=&owner;
  owner.frame=3;owner.task=2;owner.flush=3;
  A04DisplayResources r={};r.SharedCommonOwner=&owner;r.GpuHandle=&r;r.Lease.token=17;
  r.DirectPostActive=r.LiveReady=1;r.LiveShadowValid=1;
  void *pixels=calloc(1,4096000);int result=r.SubmitLiveFrame(pixels,3200);
  assert(allocs==0 && owner.cpu && owner.shared.mdl==&owner && owner.mapRegisterBase==&owner);
  if(i==0){assert(result==0 && !r.LiveCpuFallback && r.LiveFrames==1 && !retire_calls);}
  else if(i==2 || i==5){assert(result<0 && result!=STATUS_NOT_SUPPORTED && r.LiveFaulted && !r.LiveCpuFallback);}
  else {
   assert(result==STATUS_NOT_SUPPORTED && r.LiveCpuFallback && !r.LiveReady && !r.LiveFaulted && !r.LiveShadowValid);
   assert(retire_calls==1);int previous=calls;
   assert(r.SubmitLiveFrame(pixels,3200)==STATUS_NOT_SUPPORTED && calls==previous);
   assert(r.RetireToCpu()==0 && calls==previous);
  }
  assert(proof_marks==((i && A04_GE8300_IDLE_TRANSITION_DIAGNOSTIC)?1:0));
  free(pixels);free(owner.cpu);
 }
 return 0;
}
'''
# Verify live DOD restores the complete intended POST image after retirement;
# it must not fall through to dirty-only CPU writes after a partial GPU copy.
bdd=(g/'wddm-display/bdd.cxx').read_text();blt=(g/'wddm-display/blthw.cxx').read_text()
assert 'status==STATUS_NOT_SUPPORTED && resources->LiveCpuFallback' in bdd
assert 'WRITE_REGISTER_BUFFER_ULONG((volatile ULONG *)mode->FrameBuffer.Ptr,(ULONG *)staging,1024000u);' in bdd
assert 'if(presentStatus==STATUS_NOT_SUPPORTED)' in blt
power=fn(bdd,'NTSTATUS BASIC_DISPLAY_DRIVER::SetPowerState(')
assert 'RetireToCpu(' not in power and 'SetVidPnSourceVisibility(' not in power
assert 'A04PresentGuard powerGuard' in power and 'LiveShadowValid=FALSE' in power
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as tmp:
 tmp=Path(tmp);p=tmp/'test.cpp';p.write_text(pre+functions+main)
 for diagnostic in (0,1):
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-multichar','-Wno-misleading-indentation',f'-DA04_GE8300_IDLE_TRANSITION_DIAGNOSTIC={diagnostic}','-I'+str(g),str(p),'-o',str(tmp/'test')],check=True)
  subprocess.run([str(tmp/'test')],check=True)
print('actual DOD submission: diagnostic proof mark ON/OFF, fallback, invalid proof and retained mappings passed')
