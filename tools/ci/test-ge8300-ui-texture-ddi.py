#!/usr/bin/env python3
"""Execute the actual DDI resource descriptor validator with minimal WDK types."""
from pathlib import Path
import argparse,subprocess,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False);src=r/'drivers/gpu/mt8167/wddm-render/umd/typed_resource.c';s=src.read_text();begin=s.index('static HRESULT desc_valid(');end=s.index('\nstatic SIZE_T',begin);actual=s[begin:end]
pre=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "texture_layout.h"
typedef int32_t HRESULT;typedef unsigned UINT;
#define E_NOTIMPL -1
#define E_INVALIDARG -2
#define S_OK 0
#define A04_GE8300_UNIFIED_DISPLAY356 1
#define A04_NATIVE_FRAME_HEIGHT 1280u
#define GEO_VERTEX_MAX_BYTES 65536u
#define D3D10DDIRESOURCE_BUFFER 1
#define D3D10DDIRESOURCE_TEXTURE2D 2
#define D3D10_DDI_BIND_VERTEX_BUFFER 1
#define D3D10_DDI_BIND_INDEX_BUFFER 2
#define D3D10_DDI_BIND_CONSTANT_BUFFER 4
#define D3D10_DDI_BIND_RENDER_TARGET 8
#define D3D10_DDI_BIND_SHADER_RESOURCE 16
#define D3D10_DDI_BIND_PRESENT 32
#define D3D10_DDI_USAGE_DEFAULT 0
#define D3D10_DDI_USAGE_IMMUTABLE 1
#define D3D10_DDI_USAGE_DYNAMIC 2
#define D3D10_DDI_USAGE_STAGING 3
#define D3D10_DDI_CPU_ACCESS_MASK 3
#define D3D10_DDI_CPU_ACCESS_WRITE 1
#define DXGI_FORMAT_UNKNOWN 0
#define DXGI_FORMAT_B8G8R8A8_UNORM 87
#define DXGI_FORMAT_R8G8B8A8_UNORM 28
struct primary{UINT VidPnSourceId;struct{UINT Width,Height;}ModeDesc;};
typedef struct{UINT TexelWidth,TexelHeight,TexelDepth,PhysicalWidth,PhysicalHeight,PhysicalDepth;}D3D10DDI_MIPINFO;
typedef struct{D3D10DDI_MIPINFO*pMipInfoList;UINT MipLevels,ArraySize,MiscFlags,ByteStride,DecoderBufferType,BindFlags,Usage,MapFlags,ResourceDimension,Format;struct primary*pPrimaryDesc;struct{UINT Count,Quality;}SampleDesc;void*pInitialDataUP;}D3D11DDIARG_CREATERESOURCE;
'''
post=r'''
int main(void){unsigned cases=0;D3D10DDI_MIPINFO mip={257,129,1,257,129,1};D3D11DDIARG_CREATERESOURCE p={0};p.pMipInfoList=&mip;p.MipLevels=p.ArraySize=p.SampleDesc.Count=1;p.ResourceDimension=2;p.Format=87;p.BindFlags=D3D10_DDI_BIND_SHADER_RESOURCE;
 for(unsigned format=0;format<2;format++){
  p.Format=format?28:87;assert(!desc_valid(&p));cases++;
  mip.TexelWidth=mip.PhysicalWidth=16384;mip.TexelHeight=mip.PhysicalHeight=1;assert(!desc_valid(&p));cases++;
  mip.TexelHeight=mip.PhysicalHeight=65;assert(desc_valid(&p)!=0);cases++;
  mip.TexelWidth=mip.PhysicalWidth=257;mip.TexelHeight=mip.PhysicalHeight=129;
  p.MipLevels=2;assert(desc_valid(&p)!=0);p.MipLevels=1;cases++;
  p.SampleDesc.Count=4;assert(desc_valid(&p)!=0);p.SampleDesc.Count=1;cases++;
  p.BindFlags|=D3D10_DDI_BIND_RENDER_TARGET;assert(desc_valid(&p)!=0);p.BindFlags=D3D10_DDI_BIND_SHADER_RESOURCE;cases++;
 }
 p.Format=29;assert(desc_valid(&p)!=0);cases++;
 p.Format=87;p.BindFlags=D3D10_DDI_BIND_RENDER_TARGET;mip.TexelWidth=mip.PhysicalWidth=800;mip.TexelHeight=mip.PhysicalHeight=1280;assert(!desc_valid(&p));cases++;
 p.Format=28;assert(desc_valid(&p)!=0);cases++;
 printf("actual typed CreateResource validator: %u NPOT/pitch-budget/BGRA-RGBA admission and unchanged rejection cases PASS; WDK types mocked\n",cases);return 0;}
'''
f=o/'actual-ddi.c';f.write_text(pre+actual+post);x=subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'drivers/gpu/mt8167/wddm-render'),str(f),'-o',str(o/'test')],capture_output=True,text=True);(o/'compile.log').write_text(x.stdout+x.stderr);x.check_returncode();x=subprocess.run([str(o/'test')],capture_output=True,text=True);(o/'test.log').write_text(x.stdout+x.stderr);print(x.stdout+x.stderr,end='');x.check_returncode();(o/'receipt.json').write_text(json.dumps({'actual_source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'actual_desc_valid':True,'ASAN_UBSAN':True,'WDK_types_mocked':True,'hardware_verified':False},indent=2)+'\n')
