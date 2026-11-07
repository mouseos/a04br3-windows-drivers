#!/usr/bin/env python3
"""Run actual logical-present recovery; partial GPU writes require full restore."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'drivers/gpu/mt8167/wddm-display/bdd.cxx').read_text()
a=s.index('NTSTATUS BASIC_DISPLAY_DRIVER::TryGpuLogicalPresent(');b=s.index('{',a);e=b+1;n=1
while n:n+=(s[e]=='{')-(s[e]=='}');e+=1
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "idle_transition_diagnostic.h"
using NTSTATUS=int;using ULONG=uint32_t;using UCHAR=unsigned char;using BOOLEAN=int;
#define TRUE 1
#define FALSE 0
#define NT_SUCCESS(s) ((s)>=0)
#define STATUS_SUCCESS 0
#define STATUS_NOT_SUPPORTED (-1)
#define STATUS_INSUFFICIENT_RESOURCES (-2)
#define STATUS_DATA_ERROR (-3)
#define PowerDeviceD0 1
#define D3DKMDT_VPPR_IDENTITY 1
#define D3DKMDT_VPPR_ROTATE90 2
#define D3DKMDT_VPPR_ROTATE180 3
#define D3DKMDT_VPPR_ROTATE270 4
#define D3DKMDT_VPPS_CENTERED 1
#define NonPagedPoolNx 0
struct RECT {int left,top,right,bottom;};struct D3DKMT_MOVE_RECT {RECT DestRect;};
struct BLT_INFO {void *pBits;unsigned Width,Height,Pitch,BitsPerPel,Rotation;};
struct CURRENT_BDD_MODE {struct {uint32_t Width,Height,Pitch;struct {uint64_t QuadPart;} PhysicAddress;} DispInfo;
 struct {void *Ptr;} FrameBuffer;uint32_t SrcModeWidth,SrcModeHeight,Scaling;};
static int scenario,writes,allocs,readbacks,marks;static NTSTATUS matches_mark,verify_mark;
static void KeMemoryBarrier(){}
static ULONG READ_REGISTER_ULONG(volatile ULONG *p){++readbacks;return *p ^ ((scenario==4 && readbacks==1)?1u:0u);}
static void A04DisplayTrace(ULONG stage,NTSTATUS status){++marks;if(stage==0xc5)matches_mark=status;if(stage==0xc6)verify_mark=status;}
static void *ExAllocatePoolWithTag(int,size_t n,int){++allocs;return malloc(n);}
static void ExFreePoolWithTag(void*p,int){--allocs;free(p);}
#define RtlCopyMemory std::memcpy
static void WRITE_REGISTER_BUFFER_ULONG(volatile ULONG*d,ULONG*s,ULONG n){++writes;assert(n==1024000);memcpy((void*)d,s,n*4);}
static int KeReadStateEvent(int*p){return *p;}
static int a04_present_layout(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,unsigned,bool,uint32_t*w,uint32_t*h,uint32_t*o){*w=800;*h=1280;*o=0;return 0;}
static void BltBits(BLT_INFO*d,const BLT_INFO*,int,const RECT*){((uint32_t*)d->pBits)[0]=0xaabbccdd;}
struct A04DisplayResources {
 int LiveCpuFallback=0,LiveReady=1,LiveShadowValid=0,LiveFaulted=0;bool IdleRestoreChecked=false;void *LiveShadow=nullptr;
 unsigned LiveWidth=0,LiveHeight=0,LiveRotation=0,LiveOffset=0;struct {uint64_t QuadPart=1;} PostPhys;
 void *fb;
 int RetireToCpu(){if(scenario==2)return -4;LiveCpuFallback=1;LiveReady=0;return 0;}
 int SubmitLiveFrame(const void*,uint32_t){
  ((uint32_t*)fb)[500000]=0xdeadbeef; /* A partially written POST. */
  if(scenario==1 || scenario==4){LiveCpuFallback=1;LiveReady=0;return STATUS_NOT_SUPPORTED;}
  return -4; /* No proof: CPU must not touch POST. */
 }
};
struct BASIC_DISPLAY_DRIVER {
 CURRENT_BDD_MODE m_CurrentModes[1]={};A04DisplayResources m_DisplayResources;
 struct {bool DriverStarted=true;} m_Flags;int m_AdapterPowerState=1,m_MonitorPowerState=1,m_SharedWorkerStop=0;
 NTSTATUS TryGpuLogicalPresent(const BLT_INFO*,const BLT_INFO*,ULONG,const D3DKMT_MOVE_RECT*,ULONG,const RECT*);
};
'''
main=r'''
int main(){
 for(int i=1;i<=4;i++){
  scenario=i;writes=allocs=readbacks=marks=0;matches_mark=verify_mark=-9;BASIC_DISPLAY_DRIVER d;auto &m=d.m_CurrentModes[0];auto &r=d.m_DisplayResources;
  m.DispInfo.Width=800;m.DispInfo.Height=1280;m.DispInfo.Pitch=3200;m.DispInfo.PhysicAddress.QuadPart=1;
  m.FrameBuffer.Ptr=calloc(1,4096000);m.SrcModeWidth=800;m.SrcModeHeight=1280;r.fb=m.FrameBuffer.Ptr;
  BLT_INFO src={m.FrameBuffer.Ptr,800,1280,3200,32,1},dst=src;RECT rect={0,0,1,1};
  if(i==3)r.LiveCpuFallback=1;
  int result=d.TryGpuLogicalPresent(&src,&dst,0,nullptr,1,&rect);
  auto *pixels=(uint32_t*)m.FrameBuffer.Ptr;
  if(i==1){assert(result==0 && writes==1 && pixels[0]==0xaabbccdd && pixels[500000]==0);}
  if(i==2){assert(result==-4 && writes==0 && pixels[0]==0 && pixels[500000]==0xdeadbeef);}
  if(i==3){assert(result==STATUS_NOT_SUPPORTED && writes==0 && pixels[500000]==0);}
  if(i==4){assert(result==(A04_GE8300_IDLE_TRANSITION_DIAGNOSTIC?STATUS_DATA_ERROR:0));assert(writes==1);}
  if((i==1 || i==4) && A04_GE8300_IDLE_TRANSITION_DIAGNOSTIC) {
    assert(readbacks==1024000 && marks==2 && matches_mark==(i==4?1023999:1024000));
    assert(verify_mark==(i==4?STATUS_DATA_ERROR:0) && r.IdleRestoreChecked);
    /* A second valid restore never repeats diagnostic readback or marks. */
    r.LiveCpuFallback=0;r.LiveReady=1;r.LiveFaulted=0;
    assert(d.TryGpuLogicalPresent(&src,&dst,0,nullptr,1,&rect)==0);
    assert(readbacks==1024000 && marks==2);
  } else assert(!readbacks && !marks);
  if(r.LiveShadow)ExFreePoolWithTag(r.LiveShadow,0);
  assert(!allocs);free(m.FrameBuffer.Ptr);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as tmp:
 tmp=Path(tmp);p=tmp/'test.cpp';p.write_text(pre+s[a:e]+main)
 for diagnostic in (0,1):
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-multichar','-Wno-misleading-indentation',f'-DA04_GE8300_IDLE_TRANSITION_DIAGNOSTIC={diagnostic}','-I'+str(r/'drivers/gpu/mt8167'),str(p),'-o',str(tmp/'test')],check=True)
  subprocess.run([str(tmp/'test')],check=True)
print('actual logical DOD present: full restore, diagnostic-only one-shot readback, match/mismatch and proof rejection passed')
