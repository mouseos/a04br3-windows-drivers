#!/usr/bin/env python3
"""Execute the actual DOD power callback; no provider IOCTL is stubbed."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];g=r/'drivers/gpu/mt8167'
s=(g/'wddm-display/bdd.cxx').read_text();a=s.index('NTSTATUS BASIC_DISPLAY_DRIVER::SetPowerState(');b=s.index('{',a);e=b+1;n=1
while n:n+=(s[e]=='{')-(s[e]=='}');e+=1
code=r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include "present_trace_policy.h"
#define A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC 1
#define A04_GE8300_WDDM_LIVE_FULL_PRESENT 1
#define _In_
#define PAGED_CODE() ((void)0)
#define BDD_ASSERT(x) assert(x)
#define UNREFERENCED_PARAMETER(x) (void)(x)
#define NT_SUCCESS(x) ((x)>=0)
#define STATUS_SUCCESS 0
#define MAX_CHILDREN 1
#define MAX_VIEWS 1
#define DISPLAY_ADAPTER_HW_ID 0xffffffffU
#define TRUE 1
#define Executive 0
#define KernelMode 0
#define FALSE 0
using NTSTATUS=int;using ULONG=unsigned;using UINT=unsigned;using POWER_ACTION=int;
enum DEVICE_POWER_STATE {PowerDeviceD0=1,PowerDeviceD1,PowerDeviceD2,PowerDeviceD3};
static int locked,stops;
static int KeWaitForSingleObject(void*,int,int,int,void*){assert(!locked);locked=1;return 0;}
struct A04PresentGuard {A04PresentGuard(void*){assert(locked);}~A04PresentGuard(){locked=0;}};
static void A04DisplayTrace(int,int){}
struct BASIC_DISPLAY_DRIVER {
 DEVICE_POWER_STATE m_AdapterPowerState=PowerDeviceD0,m_MonitorPowerState=PowerDeviceD0;
 int m_PresentMutex;
 struct {bool LiveShadowValid;} m_DisplayResources={true};
 struct {struct {bool SourceNotVisible,FullscreenPresent;} Flags;} m_CurrentModes[1]={};
 void StopSharedDiagnosticWorker(){assert(!locked);++stops;}
 NTSTATUS SetPowerState(ULONG,DEVICE_POWER_STATE,POWER_ACTION);
};
'''+s[a:e]+r'''
int main(){
 BASIC_DISPLAY_DRIVER d;
 assert(d.SetPowerState(DISPLAY_ADAPTER_HW_ID,PowerDeviceD3,0)==0 && !locked && stops==1);
 assert(d.m_AdapterPowerState==PowerDeviceD3 && !d.m_DisplayResources.LiveShadowValid);
 d.m_DisplayResources.LiveShadowValid=true;
 assert(d.SetPowerState(DISPLAY_ADAPTER_HW_ID,PowerDeviceD0,0)==0 && !locked && stops==1);
 assert(d.m_CurrentModes[0].Flags.SourceNotVisible && d.m_CurrentModes[0].Flags.FullscreenPresent);
 assert(!d.m_DisplayResources.LiveShadowValid && d.m_AdapterPowerState==PowerDeviceD0);
 assert(d.SetPowerState(0,PowerDeviceD3,0)==0 && d.m_MonitorPowerState==PowerDeviceD3 && !locked);
 assert(d.SetPowerState(0,PowerDeviceD0,0)==0 && d.m_MonitorPowerState==PowerDeviceD0 && !locked);
 for(unsigned i=1;i<=8;i++)assert(a04_gpu_present_trace(i,0)==(A04_GE8300_PRESENT_TRACE_ALL || i<=3));
 assert(a04_gpu_present_trace(UINT32_MAX,-1));return 0;
}
'''
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as tmp:
 tmp=Path(tmp);p=tmp/'test.cpp';p.write_text(code)
 for trace in (0,1):
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',f'-DA04_GE8300_PRESENT_TRACE_ALL={trace}','-I'+str(g),str(p),'-o',str(tmp/'test')],check=True)
  subprocess.run([str(tmp/'test')],check=True)
print('actual DOD power: D3/D0, monitor state, serialization, shadow invalidation; trace limit/all/failure passed')
