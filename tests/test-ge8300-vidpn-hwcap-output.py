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
