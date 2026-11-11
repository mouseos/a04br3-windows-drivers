#!/usr/bin/env python3
"""実callback抽出とWDK8.1型で、汚した出力bufferのReserved契約を検証する。PnP/GPU実行なし。"""
import argparse
import json
from pathlib import Path
import re
import subprocess

ap = argparse.ArgumentParser()
ap.add_argument('--wdk-shared', type=Path, required=True)
ap.add_argument('--output', type=Path, required=True)
ap.add_argument('--target-contract', action='store_true', help='実DMM/POST callbackの固定パネル契約も検証')
a = ap.parse_args()
repo = Path(__file__).resolve().parents[1]
a.output.mkdir(parents=True, exist_ok=False)
rel = 'drivers/gpu/mt8167/wddm-display/bdd.cxx'
baseline = subprocess.check_output(['git', '-C', str(repo), 'show', '3d4a3d10:' + rel], text=True)
candidate = (repo / rel).read_text()

def body(text):
    begin = text.index('NTSTATUS BASIC_DISPLAY_DRIVER::QueryVidPnHWCapability(')
    end = text.index('NTSTATUS BASIC_DISPLAY_DRIVER::GetEdid(', begin)
    return text[begin:end].replace('NTSTATUS BASIC_DISPLAY_DRIVER::QueryVidPnHWCapability(', 'static NTSTATUS QueryVidPnHWCapability(')

def wdk_type(filename, name):
    text = (a.wdk_shared / filename).read_text()
    match = re.search(r'typedef struct _' + name + r'\s*\{.*?\}\s*' + name + ';', text, re.S)
    if not match:
        raise ValueError('WDK型欠落: ' + name)
    return match.group(0)

prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef unsigned UINT;
typedef int NTSTATUS;
typedef uintptr_t D3DKMDT_HVIDPN;
typedef UINT D3DDDI_VIDEO_PRESENT_SOURCE_ID;
typedef UINT D3DDDI_VIDEO_PRESENT_TARGET_ID;
#define IN
#define OUT
#define _Inout_
#define PAGED_CODE() ((void)0)
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#define BDD_ASSERT(x) assert(x)
#define MAX_VIEWS 1
#define MAX_CHILDREN 1
#define STATUS_SUCCESS 0
#define RtlZeroMemory(p,n) memset(p,0,n)
'''
prefix += wdk_type('d3dkmdt.h', 'D3DKMDT_VIDPN_HW_CAPABILITY') + '\n'
prefix += wdk_type('d3dkmddi.h', 'DXGKARG_QUERYVIDPNHWCAPABILITY') + '\n'

main = r'''
int main() {
 _Static_assert(sizeof(D3DKMDT_VIDPN_HW_CAPABILITY)==4,"WDK bitfield size");
 const unsigned patterns[]={0u,0xffffffffu,0xa5a5a5a5u};
 for(unsigned i=0;i<3;i++) {
  unsigned pattern=patterns[i];
  struct { uint32_t before; DXGKARG_QUERYVIDPNHWCAPABILITY arg; uint32_t after; } box;
  memset(&box,0,sizeof(box));box.before=0x12345678;box.after=0x87654321;
  box.arg.hFunctionalVidPn=0x1234;box.arg.SourceId=0;box.arg.TargetId=0;
  memcpy(&box.arg.VidPnHWCaps,&pattern,sizeof(pattern));
  assert(QueryVidPnHWCapability(&box.arg)==STATUS_SUCCESS);
  assert(box.arg.hFunctionalVidPn==0x1234 && !box.arg.SourceId && !box.arg.TargetId);
  assert(box.before==0x12345678 && box.after==0x87654321);
  D3DKMDT_VIDPN_HW_CAPABILITY caps=box.arg.VidPnHWCaps;
  assert(caps.DriverRotation==(A04_GE8300_UNIFIED_DISPLAY356?0u:1u));
  assert(caps.DriverColorConvert==(A04_GE8300_UNIFIED_DISPLAY356?0u:1u));
  assert(!caps.DriverScaling && !caps.DriverCloning && !caps.DriverLinkedAdapaterOutput && !caps.DriverRemoteDisplay);
#if EXPECT_BASELINE
  assert(caps.Reserved==(pattern>>6));
#else
  assert(caps.Reserved==0);
#endif
 }
 puts(EXPECT_BASELINE?"BASELINE_RESERVED_LEAK_REPRODUCED":"CANDIDATE_RESERVED_ZERO_INPUTS_AND_CANARIES_PRESERVED");
}
'''
records = []
for label, source in [('baseline', baseline), ('candidate', candidate)]:
    for unified in (0, 1):
        stem = a.output / (label + '-' + str(unified))
        unit = stem.with_suffix('.c')
        unit.write_text(prefix + body(source) + main)
        subprocess.run(['clang', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                        '-DA04_GE8300_UNIFIED_DISPLAY356=' + str(unified),
                        '-DEXPECT_BASELINE=' + str(int(label == 'baseline')), str(unit), '-o', str(stem)], check=True)
        result = subprocess.run([str(stem)], capture_output=True, text=True, check=True)
        print(label, unified, result.stdout.strip())
        records.append({'source': label, 'unified': unified, 'exit': result.returncode, 'stdout': result.stdout.strip()})
(a.output / 'result.json').write_text(json.dumps({'actual_callback_body': True, 'actual_WDK_types': True,
    'cases': 12, 'runs': records, 'Windows_ARM_build': False, 'hardware_verified': False}, indent=2) + '\n')

# モードセットAPIだけを置換し、callback本体はsourceから取り出す。
# Windows kernel/ARM ABIや実機動作を検証するテストではない。
if a.target_contract:
    def method(source, name, return_type='NTSTATUS'):
        start = source.index(return_type + ' BASIC_DISPLAY_DRIVER::' + name + '(')
        brace = source.index('{', start)
        depth = 1
        end = brace + 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        return source[start:end] + '\n'

    dmm = (repo / 'drivers/gpu/mt8167/wddm-display/bdd_dmm.cxx').read_text()
    impl = dmm[dmm.index('class A04DmmGuard'):dmm.index('// Check pinned source')]
    methods = ['IsSupportedVidPn', 'EnumVidPnCofuncModality', 'CommitVidPn',
               'BuildPanelSignal', 'IsVidPnSourceModeFieldsValid', 'IsVidPnPathFieldsValid']
    impl += ''.join(method(dmm, name) for name in methods)
    impl += method(candidate, 'StopDeviceAndReleasePostDisplayOwnership')
    impl += method(candidate, 'QueryChildRelations')
    impl += method(candidate, 'FindSourceForTarget', 'D3DDDI_VIDEO_PRESENT_SOURCE_ID')
    signal = wdk_type('d3dkmdt.h', 'D3DKMDT_VIDEO_SIGNAL_INFO')
    fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
using UINT=uint32_t; using ULONG=uint32_t; using NTSTATUS=int32_t; using BOOLEAN=bool;
using SIZE_T=size_t; using D3DKMDT_HVIDPN=unsigned;
using D3DKMDT_HVIDPNTOPOLOGY=unsigned; using D3DKMDT_HVIDPNSOURCEMODESET=unsigned;
using D3DKMDT_HVIDPNTARGETMODESET=unsigned; using D3DDDI_VIDEO_PRESENT_SOURCE_ID=unsigned;
using D3DDDI_VIDEO_PRESENT_TARGET_ID=unsigned; using D3DDDIFORMAT=unsigned;
using D3DKMDT_VIDPN_PRESENT_PATH_SCALING=unsigned; using D3DKMDT_VIDPN_PRESENT_PATH_ROTATION=unsigned;
using D3DKMDT_VIDEO_SIGNAL_STANDARD=unsigned; using D3DDDI_VIDEO_SIGNAL_SCANLINE_ORDERING=unsigned;
struct D3DKMDT_2DREGION { UINT cx,cy; };
struct D3DDDI_RATIONAL { UINT Numerator,Denominator; };
#define DXGKDDI_INTERFACE_VERSION 0x4002
#define DXGKDDI_INTERFACE_VERSION_WDDM1_3_M1 0x4000
#define CONST const
#define _In_
#define _Inout_
#define _Out_
#define _Out_writes_bytes_(x)
#define PAGED_CODE() ((void)0)
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#define BDD_ASSERT(x) assert(x)
#define BDD_ASSERT_CHK(x) assert(x)
#define NT_ASSERT(x) assert(x)
#define BDD_LOG_ERROR1(...) ((void)0)
#define BDD_LOG_ERROR2(...) ((void)0)
#define BDD_LOG_ERROR3(...) ((void)0)
#define BDD_LOG_ERROR4(...) ((void)0)
#define BDD_LOG_ASSERTION3(...) ((void)0)
#define RtlZeroMemory(p,n) memset(p,0,n)
#define ARRAYSIZE(x) (sizeof(x)/sizeof((x)[0]))
#define NT_SUCCESS(x) ((x)>=0)
#define STATUS_SUCCESS 0
#define STATUS_NOT_FOUND (-1)
#define STATUS_INVALID_PARAMETER (-2)
#define STATUS_DEVICE_NOT_READY (-3)
#define STATUS_DEVICE_CONFIGURATION_ERROR (-4)
#define STATUS_INVALID_DEVICE_STATE (-5)
#define STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET 0x401e034c
#define STATUS_GRAPHICS_SOURCE_NOT_IN_TOPOLOGY (-7)
#define STATUS_GRAPHICS_MODE_ALREADY_IN_MODESET (-8)
#define STATUS_GRAPHICS_VIDPN_MODALITY_NOT_SUPPORTED (-9)
#define STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE_MODE (-10)
#define STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE (-11)
#define STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_TARGET (-12)
#define STATUS_GRAPHICS_GAMMA_RAMP_NOT_SUPPORTED (-13)
#define D3DKMDT_FREQUENCY_NOTSPECIFIED 0xfffffffeu
#define D3DDDI_ID_UNINITIALIZED 0xffffffffu
#define MAX_CHILDREN 1
#define MAX_VIEWS 1
#define TRUE true
#define FALSE false
#define Executive 0
#define KernelMode 0
#define DXGK_VIDPN_INTERFACE_VERSION_V1 1
#define D3DDDIFMT_A8R8G8B8 21
#define TypeVideoOutput 1
#define HpdAwarenessInterruptible 1
#define D3DKMDT_VOT_INTERNAL 0x80000000u
#define D3DKMDT_VOT_OTHER 0xffffffffu
#define D3DKMDT_MOA_NONE 0
#define D3DKMDT_VPPR_IDENTITY 1
#define D3DKMDT_VPPR_ROTATE90 2
#define D3DKMDT_VPPR_ROTATE180 3
#define D3DKMDT_VPPR_ROTATE270 4
#define D3DKMDT_VPPR_UNPINNED 254
#define D3DKMDT_VPPR_NOTSPECIFIED 255
#define D3DKMDT_VPPR_UNINITIALIZED 0
#define D3DKMDT_VPPS_IDENTITY 1
#define D3DKMDT_VPPS_CENTERED 2
#define D3DKMDT_VPPS_UNPINNED 254
#define D3DKMDT_VPPS_NOTSPECIFIED 255
#define D3DKMDT_VPPS_UNINITIALIZED 0
#define D3DKMDT_CB_SCRGB 2
#define D3DKMDT_CB_UNINITIALIZED 0
#define D3DKMDT_PVAM_DIRECT 1
#define D3DKMDT_RMT_GRAPHICS 1
#define D3DKMDT_VSS_OTHER 255
#define D3DDDI_VSSLO_PROGRESSIVE 1
#define D3DDDI_GAMMARAMP_DEFAULT 0
#define D3DKMDT_EPT_VIDPNSOURCE 1
#define D3DKMDT_EPT_VIDPNTARGET 2
#define D3DKMDT_EPT_SCALING 3
#define D3DKMDT_EPT_ROTATION 4
#define PowerDeviceD0 1
#define PowerActionNone 0
struct KMUTEX { int held=0; };
static NTSTATUS KeWaitForSingleObject(KMUTEX *m,int,int,bool,void*){assert(!m->held);m->held++;return 0;}
static void KeReleaseMutex(KMUTEX *m,bool){assert(m->held==1);m->held--;}
'''
    fixture += signal + '\n'
    fixture += r'''
struct DXGK_CHILD_DESCRIPTOR { UINT ChildDeviceType;
 struct { UINT HpdAwareness; struct { struct { UINT InterfaceTechnology,MonitorOrientationAwareness; bool SupportsSdtvModes; } VideoOutput; } Type; } ChildCapabilities;
 UINT AcpiUid,ChildUid;
};
struct DXGK_DISPLAY_INFORMATION { UINT Width,Height,Pitch,ColorFormat; uint64_t PhysicAddress; UINT TargetId,AcpiId; };
struct CURRENT_BDD_MODE {
 DXGK_DISPLAY_INFORMATION DispInfo; struct { void *Ptr; } FrameBuffer;
 struct { bool DoNotMapOrUnmap,FrameBufferIsActive,IsInternal; } Flags;
};
struct D3DKMDT_VIDPN_SOURCE_MODE {
 UINT Type; struct { struct { D3DKMDT_2DREGION PrimSurfSize; UINT Stride,PixelFormat,ColorBasis,PixelValueAccessMode; } Graphics; } Format;
};
struct D3DKMDT_VIDPN_TARGET_MODE { D3DKMDT_VIDEO_SIGNAL_INFO VideoSignalInfo; };
struct D3DKMDT_VIDPN_PRESENT_PATH_SCALING_SUPPORT { UINT Identity,Centered; };
struct D3DKMDT_VIDPN_PRESENT_PATH_ROTATION_SUPPORT { UINT Identity,Rotate90,Rotate180,Rotate270; };
struct D3DKMDT_VIDPN_PRESENT_PATH {
 UINT VidPnSourceId,VidPnTargetId;
 struct { UINT Rotation,Scaling; D3DKMDT_VIDPN_PRESENT_PATH_SCALING_SUPPORT ScalingSupport;
  D3DKMDT_VIDPN_PRESENT_PATH_ROTATION_SUPPORT RotationSupport; } ContentTransformation;
 struct { UINT Type; } GammaRamp; UINT VidPnTargetColorBasis;
};
struct DXGKARG_ISSUPPORTEDVIDPN { D3DKMDT_HVIDPN hDesiredVidPn; bool IsVidPnSupported; };
struct DXGKARG_ENUMVIDPNCOFUNCMODALITY { D3DKMDT_HVIDPN hConstrainingVidPn; UINT EnumPivotType;
 struct { UINT VidPnSourceId,VidPnTargetId; } EnumPivot; };
struct DXGKARG_COMMITVIDPN { D3DKMDT_HVIDPN hFunctionalVidPn; UINT AffectedVidPnSourceId;
 struct { bool PathPoweredOff; } Flags; };
// The APIs enforce ownership and count mutations; the callbacks remain real.
struct ModeApi {
 NTSTATUS (*pfnAcquirePinnedModeInfo)(unsigned,const D3DKMDT_VIDPN_SOURCE_MODE**);
 NTSTATUS (*pfnReleaseModeInfo)(unsigned,const D3DKMDT_VIDPN_SOURCE_MODE*);
};
struct TargetApi {
 NTSTATUS (*pfnAcquirePinnedModeInfo)(unsigned,const D3DKMDT_VIDPN_TARGET_MODE**);
 NTSTATUS (*pfnReleaseModeInfo)(unsigned,const D3DKMDT_VIDPN_TARGET_MODE*);
};
using DXGK_VIDPNSOURCEMODESET_INTERFACE=ModeApi; using DXGK_VIDPNTARGETMODESET_INTERFACE=TargetApi;
struct DXGK_VIDPNTOPOLOGY_INTERFACE {
 NTSTATUS (*pfnGetNumPathsFromSource)(unsigned,UINT,SIZE_T*);
 NTSTATUS (*pfnAcquireFirstPathInfo)(unsigned,const D3DKMDT_VIDPN_PRESENT_PATH**);
 NTSTATUS (*pfnAcquireNextPathInfo)(unsigned,const D3DKMDT_VIDPN_PRESENT_PATH*,const D3DKMDT_VIDPN_PRESENT_PATH**);
 NTSTATUS (*pfnReleasePathInfo)(unsigned,const D3DKMDT_VIDPN_PRESENT_PATH*);
 NTSTATUS (*pfnUpdatePathSupportInfo)(unsigned,const D3DKMDT_VIDPN_PRESENT_PATH*);
 NTSTATUS (*pfnGetNumPaths)(unsigned,SIZE_T*);
 NTSTATUS (*pfnEnumPathTargetsFromSource)(unsigned,UINT,UINT,UINT*);
 NTSTATUS (*pfnAcquirePathInfo)(unsigned,UINT,UINT,const D3DKMDT_VIDPN_PRESENT_PATH**);
};
struct DXGK_VIDPN_INTERFACE {
 NTSTATUS (*pfnGetTopology)(unsigned,unsigned*,const DXGK_VIDPNTOPOLOGY_INTERFACE**);
 NTSTATUS (*pfnAcquireSourceModeSet)(unsigned,UINT,unsigned*,const ModeApi**);
 NTSTATUS (*pfnReleaseSourceModeSet)(unsigned,unsigned);
 NTSTATUS (*pfnAcquireTargetModeSet)(unsigned,UINT,unsigned*,const TargetApi**);
 NTSTATUS (*pfnReleaseTargetModeSet)(unsigned,unsigned);
 NTSTATUS (*pfnCreateNewSourceModeSet)(unsigned,UINT,unsigned*,const ModeApi**);
 NTSTATUS (*pfnAssignSourceModeSet)(unsigned,UINT,unsigned);
 NTSTATUS (*pfnCreateNewTargetModeSet)(unsigned,UINT,unsigned*,const TargetApi**);
 NTSTATUS (*pfnAssignTargetModeSet)(unsigned,UINT,unsigned);
};
static D3DKMDT_VIDPN_SOURCE_MODE source;
static D3DKMDT_VIDPN_TARGET_MODE target;
static D3DKMDT_VIDPN_PRESENT_PATH path;
static bool sourcePin,targetPin,empty;
static int paths,srcsets,dstsets,srcmodes,dstmodes,mutations,mapped,cleaned,blackouts,failAt;
static NTSTATUS acquireSrc(unsigned,const D3DKMDT_VIDPN_SOURCE_MODE **p){*p=sourcePin?&source:nullptr;srcmodes+=sourcePin;return sourcePin?0:1;}
static NTSTATUS releaseSrc(unsigned,const D3DKMDT_VIDPN_SOURCE_MODE *p){assert(p==&source);assert(srcmodes--==1);return 0;}
static NTSTATUS acquireDst(unsigned,const D3DKMDT_VIDPN_TARGET_MODE **p){if(failAt==1)return -99;*p=targetPin?&target:nullptr;dstmodes+=targetPin;return targetPin?0:1;}
static NTSTATUS releaseDst(unsigned,const D3DKMDT_VIDPN_TARGET_MODE *p){assert(p==&target);assert(dstmodes--==1);return failAt==2?-99:0;}
static ModeApi srcapi={acquireSrc,releaseSrc}; static TargetApi dstapi={acquireDst,releaseDst};
static NTSTATUS countSource(unsigned,UINT id,SIZE_T *n){assert(id==0);*n=empty?0:1;return empty?STATUS_GRAPHICS_SOURCE_NOT_IN_TOPOLOGY:0;}
static NTSTATUS countPaths(unsigned,SIZE_T *n){*n=empty?0:1;return 0;}
static NTSTATUS first(unsigned,const D3DKMDT_VIDPN_PRESENT_PATH **p){*p=empty?nullptr:&path;if(empty)return STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET;paths++;return 0;}
static NTSTATUS next(unsigned,const D3DKMDT_VIDPN_PRESENT_PATH *p,const D3DKMDT_VIDPN_PRESENT_PATH **n){assert(p==&path);*n=nullptr;return STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET;}
static NTSTATUS releasePath(unsigned,const D3DKMDT_VIDPN_PRESENT_PATH *p){assert(p==&path);assert(paths--==1);return 0;}
static NTSTATUS updatePath(unsigned,const D3DKMDT_VIDPN_PRESENT_PATH *p){path=*p;mutations++;return 0;}
static NTSTATUS enumTarget(unsigned,UINT id,UINT index,UINT *p){assert(!id&&!index);*p=0;return 0;}
static NTSTATUS getPath(unsigned h,UINT s,UINT t,const D3DKMDT_VIDPN_PRESENT_PATH **p){assert(!s&&!t);return first(h,p);}
static DXGK_VIDPNTOPOLOGY_INTERFACE topo={countSource,first,next,releasePath,updatePath,countPaths,enumTarget,getPath};
static NTSTATUS topology(unsigned,unsigned *h,const DXGK_VIDPNTOPOLOGY_INTERFACE **p){*h=1;*p=&topo;return 0;}
static NTSTATUS getSource(unsigned,UINT id,unsigned *h,const ModeApi **p){assert(!id);srcsets++;*h=1;*p=&srcapi;return 0;}
static NTSTATUS putSource(unsigned,unsigned){assert(srcsets--==1);return 0;}
static NTSTATUS getTarget(unsigned,UINT id,unsigned *h,const TargetApi **p){assert(!id);if(failAt==3)return -99;dstsets++;*h=1;*p=&dstapi;return 0;}
static NTSTATUS putTarget(unsigned,unsigned){assert(dstsets--==1);return failAt==4?-99:0;}
static NTSTATUS assignSource(unsigned h,UINT,unsigned set){mutations++;return putSource(h,set);}
static NTSTATUS assignTarget(unsigned h,UINT,unsigned set){mutations++;return putTarget(h,set);}
static DXGK_VIDPN_INTERFACE vidpn={topology,getSource,putSource,getTarget,putTarget,getSource,assignSource,getTarget,assignTarget};
static NTSTATUS query(unsigned h,UINT,const DXGK_VIDPN_INTERFACE **p){assert(h==1);*p=&vidpn;return 0;}
struct BASIC_DISPLAY_DRIVER {
 struct { NTSTATUS (*DxgkCbQueryVidPnInterface)(unsigned,UINT,const DXGK_VIDPN_INTERFACE**); } m_DxgkInterface={query};
 CURRENT_BDD_MODE m_CurrentModes[1]={}; KMUTEX m_PresentMutex;
 struct { bool DriverStarted; } m_Flags={true}; int m_MonitorPowerState=PowerDeviceD0;
 a04_panel_timing m_PanelTiming={A04_PANEL_TIMING_MAGIC,1,64,1,1,800,1280,960,1300,5990,100,{0}};
 NTSTATUS IsSupportedVidPn(DXGKARG_ISSUPPORTEDVIDPN*);
 NTSTATUS EnumVidPnCofuncModality(const DXGKARG_ENUMVIDPNCOFUNCMODALITY*);
 NTSTATUS CommitVidPn(const DXGKARG_COMMITVIDPN*);
 NTSTATUS BuildPanelSignal(D3DKMDT_VIDEO_SIGNAL_INFO*,UINT,UINT);
 NTSTATUS IsVidPnSourceModeFieldsValid(const D3DKMDT_VIDPN_SOURCE_MODE*) const;
 NTSTATUS IsVidPnPathFieldsValid(const D3DKMDT_VIDPN_PRESENT_PATH*) const;
 NTSTATUS StopDeviceAndReleasePostDisplayOwnership(UINT,DXGK_DISPLAY_INFORMATION*);
 NTSTATUS AddSingleSourceMode(const ModeApi*,unsigned,UINT){assert(!sourcePin);mutations++;return 0;}
 NTSTATUS AddSingleTargetMode(const TargetApi*,unsigned,const D3DKMDT_VIDPN_SOURCE_MODE*,UINT){assert(!targetPin);mutations++;return 0;}
 NTSTATUS SetSourceModeAndPath(const D3DKMDT_VIDPN_SOURCE_MODE*,const D3DKMDT_VIDPN_PRESENT_PATH*){mapped++;return 0;}
 NTSTATUS UnmapFrameBuffer(void*,UINT){mapped--;return 0;}
 D3DDDI_VIDEO_PRESENT_SOURCE_ID FindSourceForTarget(UINT,bool);
 NTSTATUS QueryChildRelations(DXGK_CHILD_DESCRIPTOR*,ULONG);
 void SetPowerState(UINT,int,int){assert(false);}
 NTSTATUS BlackOutScreen(UINT id){assert(!id);blackouts++;return 0;}
 void CleanUp(){cleaned++;}
};
'''
    fixture = fixture.replace('struct BASIC_DISPLAY_DRIVER {', 'using A04PresentGuard=A04DmmGuard;\nstruct BASIC_DISPLAY_DRIVER {')
    # Guard definition precedes the fixture class that aliases it.
    guard_end = impl.index('// Compare signal meaning')
    guard = impl[:guard_end]
    impl = impl[guard_end:]
    fixture = fixture.replace('using A04PresentGuard=A04DmmGuard;', guard + '\nusing A04PresentGuard=A04DmmGuard;')
    fixture += impl
    fixture += r'''
static void balanced(){assert(!paths&&!srcsets&&!dstsets&&!srcmodes&&!dstmodes);}
static void reset(BASIC_DISPLAY_DRIVER& d){
 balanced(); sourcePin=targetPin=true;empty=false;mutations=mapped=cleaned=blackouts=failAt=0;
 source={};source.Type=D3DKMDT_RMT_GRAPHICS;
 source.Format.Graphics={{800,1280},3200,21,2,1};
 path={};path.ContentTransformation.Rotation=1;path.ContentTransformation.Scaling=1;path.VidPnTargetColorBasis=2;
 d.m_Flags.DriverStarted=true;d.m_CurrentModes[0]={};
 d.m_CurrentModes[0].DispInfo={800,1280,3200,22,0xbe9c0000,0xffffffffu,0};
 assert(d.BuildPanelSignal(&target.VideoSignalInfo,800,1280)==0);
}
static void supported(BASIC_DISPLAY_DRIVER& d,bool expected){
 DXGKARG_ISSUPPORTEDVIDPN arg={1,false};assert(d.IsSupportedVidPn(&arg)==0);assert(arg.IsVidPnSupported==expected);balanced();
}
static void enumeration(BASIC_DISPLAY_DRIVER& d,UINT pivot,bool good){
 const auto savedSource=source;const auto savedTarget=target;const auto savedPath=path;
 DXGKARG_ENUMVIDPNCOFUNCMODALITY arg={1,pivot,{0,0}};
 NTSTATUS status=d.EnumVidPnCofuncModality(&arg);
 assert(good?status==0:status==STATUS_GRAPHICS_VIDPN_MODALITY_NOT_SUPPORTED);
 assert(!memcmp(&source,&savedSource,sizeof(source))&&!memcmp(&target,&savedTarget,sizeof(target)));
 assert(!memcmp(&path,&savedPath,sizeof(path))&&!mutations);balanced();
}
static void commit(BASIC_DISPLAY_DRIVER& d,bool good){
 DXGKARG_COMMITVIDPN arg={1,0,{false}};
 NTSTATUS status=d.CommitVidPn(&arg);
 assert(good?status==0:status==STATUS_GRAPHICS_VIDPN_MODALITY_NOT_SUPPORTED);
 assert(mapped==((good&&!empty)?1:0));balanced();mapped=0;
}
int main(){
 BASIC_DISPLAY_DRIVER d;
 // The captured233 signal, exact rational variants, and poisoned reserved bits.
 for(int variant=0;variant<3;variant++){
  reset(d);auto& s=target.VideoSignalInfo;
  if(variant==1){s.VSyncFreq={59900000,1000000};s.HSyncFreq={3114800000u,40000};}
  if(variant==2)s.AdditionalSignalInfo.Reserved=0x7fffff;
  supported(d,true);for(UINT pivot=0;pivot<=4;pivot++)enumeration(d,pivot,true);commit(d,true);
 }
 // Each semantic mismatch must be rejected across all three actual callbacks,
 // including a target pivot, before assignment/mapping. No pin is replaced.
 for(int bad=0;bad<12;bad++){
  reset(d);auto& s=target.VideoSignalInfo;
  switch(bad){case 0:s.ActiveSize.cx++;break;case 1:s.TotalSize.cy++;break;
  case 2:s.VSyncFreq.Numerator++;break;case 3:s.HSyncFreq.Numerator++;break;
  case 4:s.PixelRate++;break;case 5:s.AdditionalSignalInfo.ScanLineOrdering=2;break;
  case 6:s.AdditionalSignalInfo.VSyncFreqDivider=1;break;case 7:s.VideoStandard++;break;
  case 8:s.VSyncFreq.Denominator=0;break;case 9:s.HSyncFreq.Numerator=0xfffffffeu;break;
  case 10:s.VSyncFreq={0xffffffffu,0xffffffffu};break;case 11:s.HSyncFreq={0xfffffffdu,0xfffffffcu};break;}
  supported(d,false);for(UINT pivot=0;pivot<=4;pivot++)enumeration(d,pivot,false);commit(d,false);
 }
 reset(d);targetPin=false;supported(d,true);commit(d,false);
 // Pivot mode sets stay untouched even with no pin. Other non-pivot sets
 // may be populated through the existing enumeration path.
 reset(d);targetPin=false;enumeration(d,D3DKMDT_EPT_VIDPNTARGET,true);
 reset(d);sourcePin=false;enumeration(d,D3DKMDT_EPT_VIDPNSOURCE,true);commit(d,false);
 // Unpinned transforms are narrowed; pivot support fields are not overwritten.
 for(UINT pivot=D3DKMDT_EPT_SCALING;pivot<=D3DKMDT_EPT_ROTATION;pivot++){
  reset(d);path.ContentTransformation.Scaling=path.ContentTransformation.Rotation=254;
  path.ContentTransformation.ScalingSupport={7,9};path.ContentTransformation.RotationSupport={3,4,5,6};
  auto saved=path;DXGKARG_ENUMVIDPNCOFUNCMODALITY arg={1,pivot,{0,0}};
  assert(d.EnumVidPnCofuncModality(&arg)==0);balanced();
  if(pivot==D3DKMDT_EPT_SCALING)assert(!memcmp(&saved.ContentTransformation.ScalingSupport,&path.ContentTransformation.ScalingSupport,sizeof(path.ContentTransformation.ScalingSupport)));
  else assert(!memcmp(&saved.ContentTransformation.RotationSupport,&path.ContentTransformation.RotationSupport,sizeof(path.ContentTransformation.RotationSupport)));
  assert(path.ContentTransformation.Scaling==254&&path.ContentTransformation.Rotation==254);
 }
 // Large-component equality (products exceed 32 bits); overflow cannot make
 // adjacent rationals equal. Non-rate sentinels and zero denominators fail.
 assert(A04SameFrequency({0xfffffffdu,0xfffffffdu},{1,1}));
 assert(!A04SameFrequency({0xfffffffdu,0xfffffffcu},{1,1}));
 assert(!A04SameFrequency({1,0},{1,0}));
 for(int failure=1;failure<=4;failure++){
  reset(d);failAt=failure;DXGKARG_ISSUPPORTEDVIDPN s={1,true};
  assert(d.IsSupportedVidPn(&s)==-99&&!s.IsVidPnSupported);balanced();
  DXGKARG_ENUMVIDPNCOFUNCMODALITY e={1,2,{0,0}};assert(d.EnumVidPnCofuncModality(&e)==-99);balanced();
  DXGKARG_COMMITVIDPN c={1,0,{false}};assert(d.CommitVidPn(&c)==-99);balanced();assert(!mutations&&!mapped);
 }
 reset(d);empty=true;supported(d,true);commit(d,true); // disabled topology does not map
 reset(d);DXGK_DISPLAY_INFORMATION out={};DXGK_CHILD_DESCRIPTOR children[2]={};
 assert(d.QueryChildRelations(children,sizeof(children))==0);
 assert(children[0].ChildDeviceType==TypeVideoOutput&&!children[0].ChildUid&&!children[1].ChildDeviceType);
 assert(d.StopDeviceAndReleasePostDisplayOwnership(0,&out)==0);
 assert(out.TargetId==children[0].ChildUid&&out.AcpiId==children[0].AcpiUid&&out.Width==800&&out.Height==1280&&out.PhysicAddress==0xbe9c0000);
 assert(d.m_CurrentModes[0].DispInfo.TargetId==0xffffffffu&&!d.m_Flags.DriverStarted&&cleaned==1&&blackouts==1);
 reset(d);assert(d.StopDeviceAndReleasePostDisplayOwnership(1,&out)==STATUS_INVALID_PARAMETER);
 assert(!cleaned&&d.m_Flags.DriverStarted);balanced();
 puts("ACTUAL_DMM_POST_CALLBACK_CONTRACTS_PASS (mock mode-set APIs; no Windows/ARM/hardware claim)");
}
'''
    unit = a.output / 'display-target-contract.cpp'
    includes = ('#include "' + str(repo / 'drivers/gpu/mt8167/display-transport/panel_timing.h') + '"\n'
                '#include "' + str(repo / 'drivers/gpu/mt8167/wddm-display/present_bounds.h') + '"\n')
    unit.write_text(includes + fixture)
    exe = unit.with_suffix('')
    subprocess.run(['clang', '-x', 'c++', '-std=c++14', '-fno-exceptions', '-fno-rtti', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
                    '-Wno-unused-function', '-Wno-unknown-pragmas', '-fsanitize=address,undefined',
                    '-DA04_GE8300_UNIFIED_DISPLAY356=1', str(unit), '-o', str(exe)], check=True)
    result = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
    print(result.stdout.strip())
    (a.output / 'target-contract-result.json').write_text(json.dumps({
        'actual_callback_bodies': methods + ['StopDeviceAndReleasePostDisplayOwnership', 'QueryChildRelations', 'FindSourceForTarget'],
        'actual_WDK_signal_type': True, 'mode_set_APIs': 'host fixture, balanced references and mutation guards',
        'semantic_bad_signals': 12, 'pivot_types': 5, 'query_release_failures': 4,
        'output': result.stdout.strip(), 'exit': result.returncode,
        'Windows_ARM_build': False, 'hardware_verified': False}, indent=2) + '\n')
