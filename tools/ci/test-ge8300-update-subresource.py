#!/usr/bin/env python3
"""Production UpdateSubresourceUP/map/unmap transaction with bounded WDK/runtime seams."""
from pathlib import Path
import argparse,subprocess,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();R=Path(__file__).resolve().parents[2];S=R/'drivers/gpu/mt8167/wddm-render/umd';o=a.out.resolve();o.mkdir(exist_ok=False,parents=True)
s=(S/'typed_resource.c').read_text()
def section(a,b):return s[s.index(a):s.index(b,s.index(a))]
actual=section('static HRESULT map_backend(', 'static void APIENTRY create_resource(')+section('static void APIENTRY update_subresource(', 'static void APIENTRY copy_resource(')
pre=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "update_subresource.h"
typedef uint32_t UINT;typedef uint32_t D3DKMT_HANDLE;typedef int32_t HRESULT;typedef int D3D10_DDI_MAP;
#define APIENTRY
#define S_OK 0
#define E_INVALIDARG ((HRESULT)0x80070057u)
#define E_NOTIMPL ((HRESULT)0x80004001u)
#define E_UNEXPECTED ((HRESULT)0x8000ffffu)
#define E_FAIL ((HRESULT)0x80004005u)
#define FAILED(x) ((x)<0)
#define SUCCEEDED(x) ((x)>=0)
#define D3D10_DDI_MAP_READ 1
#define D3D10_DDI_MAP_WRITE 2
#define D3D10_DDI_MAP_FLAG_DONOTWAIT 1
#define D3D10_DDI_USAGE_DEFAULT 0
#define D3D10DDIRESOURCE_TEXTURE2D 3
#define D3D10_DDI_BIND_SHADER_RESOURCE 8
#define DXGI_FORMAT_B8G8R8A8_UNORM 87
#define DXGI_FORMAT_R8G8B8A8_UNORM 28
#define GEO_FORMAT_A8R8G8B8 21
#define GEO_USAGE_TEXTURE2D 6
struct geo_surface_desc {UINT width,height,pitch,format;};
typedef struct{UINT left,top,front,right,bottom,back;}D3D10_DDI_BOX;
typedef struct{void*pData;UINT RowPitch,DepthPitch;}D3D10DDI_MAPPED_SUBRESOURCE;
typedef struct{void*hResource;struct{UINT ReadOnly,DoNotWait;}Flags;void*pSurfData;UINT Pitch,SlicePitch;}D3DDDIARG_LOCK;
typedef struct{void*hResource;}D3DDDIARG_UNLOCK;
struct geo_umd_device;struct geo_typed_resource;
struct geo_umd_device{HRESULT(*frontend_flush)(struct geo_umd_device*);};
struct geo_typed_device{HRESULT error;struct geo_umd_device backend;struct{HRESULT(*pfnLock)(struct geo_umd_device*,D3DDDIARG_LOCK*);HRESULT(*pfnUnlock)(struct geo_umd_device*,D3DDDIARG_UNLOCK*);}memory;};
struct geo_typed_resource{UINT mapped,usage,dimension,bind,primary,map_flags,format,width,height;void*backend;struct geo_typed_device*device;};
typedef struct geo_typed_device*D3D10DDI_HDEVICE;typedef struct geo_typed_resource*D3D10DDI_HRESOURCE;
static uint8_t pixels[320],saved[320];static unsigned locks,unlocks,flushes,held,step;static HRESULT lock_rc,unlock_rc,info_rc;static UINT pitch=64,cap=320;static struct geo_surface_desc desc={7,5,64,21};static HRESULT last;
static struct geo_typed_device*geo_typed_device(D3D10DDI_HDEVICE h){return h;}
static struct geo_typed_resource*geo_typed_resource(D3D10DDI_HDEVICE h,D3D10DDI_HRESOURCE r){return r&&r->device==h?r:NULL;}
static void geo_typed_error(D3D10DDI_HDEVICE d,HRESULT e){last=e;if(d&&FAILED(e))d->error=e;}
static void geo_typed_device_trace(struct geo_typed_device*d,const char*k,UINT v){(void)d;(void)k;(void)v;}
static void geo_typed_resource_trace(D3D10DDI_HDEVICE d,const char*k,UINT v){(void)d;(void)k;(void)v;}
static HRESULT geo_umd_resource_info(struct geo_umd_device*d,void*r,UINT usage,struct geo_surface_desc*out,D3DKMT_HANDLE*a){(void)d;(void)r;assert(usage==GEO_USAGE_TEXTURE2D);*out=desc;*a=19;return info_rc;}
static HRESULT flush(struct geo_umd_device*d){(void)d;flushes++;assert(step==0);step=1;return S_OK;}
static HRESULT lock_(struct geo_umd_device*d,D3DDDIARG_LOCK*q){(void)d;assert(step==1&&!q->Flags.ReadOnly&&!q->Flags.DoNotWait&&!held);locks++;step=2;if(FAILED(lock_rc))return lock_rc;held=1;q->pSurfData=pixels;q->Pitch=pitch;q->SlicePitch=cap;return S_OK;}
static HRESULT unlock_(struct geo_umd_device*d,D3DDDIARG_UNLOCK*q){(void)d;(void)q;assert(step==2&&held);unlocks++;step=3;if(FAILED(unlock_rc))return unlock_rc;held=0;return S_OK;}
static void reset(struct geo_typed_device*d,struct geo_typed_resource*r){memset(d,0,sizeof(*d));memset(r,0,sizeof(*r));d->backend.frontend_flush=flush;d->memory.pfnLock=lock_;d->memory.pfnUnlock=unlock_;r->backend=r;r->device=d;r->width=7;r->height=5;r->bind=8;r->dimension=3;r->format=87;desc=(struct geo_surface_desc){7,5,64,21};locks=unlocks=flushes=held=step=0;pitch=64;cap=320;lock_rc=unlock_rc=info_rc=last=0;memset(pixels,0xa5,sizeof(pixels));memcpy(saved,pixels,sizeof(pixels));}
'''
post=r'''
int main(void){struct geo_typed_device d;struct geo_typed_resource r;D3D10_DDI_BOX b={2,1,0,5,4,1};uint8_t src[57];unsigned cases=0;memset(src,0x3c,sizeof(src));
 for(unsigned format=0;format<2;format++){
 reset(&d,&r);r.format=format?28:87;desc.format=format?32:21;
 update_subresource(&d,&r,0,&b,src,19,0xffffffffu,0);assert(!d.error&&locks==1&&unlocks==1&&!held&&!r.mapped);
 for(unsigned y=0;y<5;y++)for(unsigned x=0;x<64;x++)assert(pixels[y*64+x]==((y>=1&&y<4&&x>=8&&x<20)?0x3c:0xa5));cases++;
 step=0;memset(src,0x7e,sizeof(src));update_subresource(&d,&r,0,&b,src,19,1,0);assert(!d.error&&locks==2&&unlocks==2);assert(pixels[72]==0x7e);cases++;memset(src,0x3c,sizeof(src));
 }
 reset(&d,&r);uint8_t full[5*31];memset(full,0x66,sizeof(full));update_subresource(&d,&r,0,NULL,full,31,0,0);assert(!d.error&&pixels[4*64+27]==0x66&&pixels[28]==0xa5);cases++;
 reset(&d,&r);r.height=desc.height=1;cap=64;update_subresource(&d,&r,0,NULL,full,0,0,0);assert(!d.error&&locks==1&&pixels[27]==0x66);cases++;
 reset(&d,&r);b.right=b.left;update_subresource(&d,&r,0,&b,NULL,0,0,0);assert(!d.error&&!flushes&&!locks&&!memcmp(pixels,saved,320));b.right=5;cases++;
 for(unsigned bad=0;bad<10;bad++){
 reset(&d,&r);UINT sub=0,flags=0,p=19;D3D10_DDI_BOX box=b;
 switch(bad){case 0:sub=1;break;case 1:r.usage=1;break;case 2:r.usage=2;break;case 3:r.bind=9;break;case 4:flags=1;break;case 5:flags=2;break;case 6:box.right=8;break;case 7:box.back=2;break;case 8:p=11;break;case 9:r.map_flags=1;break;}
 update_subresource(&d,&r,sub,&box,src,p,0,flags);assert(FAILED(last)&&!locks&&!memcmp(pixels,saved,320));cases++;
 }
 reset(&d,&r);lock_rc=E_FAIL;update_subresource(&d,&r,0,&b,src,19,0,0);assert(last==E_FAIL&&locks==1&&!unlocks&&!memcmp(pixels,saved,320));cases++;
 reset(&d,&r);cap=319;update_subresource(&d,&r,0,&b,src,19,0,0);assert(FAILED(last)&&unlocks==1&&!held&&!memcmp(pixels,saved,320));cases++;
 reset(&d,&r);update_subresource(&d,&r,0,&b,pixels,19,0,0);assert(FAILED(last)&&unlocks==1&&!held&&!memcmp(pixels,saved,320));cases++;
 reset(&d,&r);unlock_rc=E_FAIL;update_subresource(&d,&r,0,&b,src,19,0,0);assert(last==E_FAIL&&r.mapped&&held);cases++;
 struct geo_update_plan plan;struct geo_update_region big={0,0,1,16384,0,1};assert(geo_update_plan_2d(1,16384,&big,UINT32_MAX,&plan)==(SIZE_MAX==UINT32_MAX?-1:0));cases++;
 printf("production update/map/unmap: %u boundary/format/second-update/synchronization-failure/retained-lock cases PASS; runtime/WDK seams mocked, hardware not tested\n",cases);
}
'''
f=o/'production-update.c';f.write_text(pre+actual+post);q=subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(S),str(f),'-o',str(o/'test')],capture_output=True,text=True);(o/'compile.log').write_text(q.stdout+q.stderr);q.check_returncode();q=subprocess.run([str(o/'test')],capture_output=True,text=True);(o/'test.log').write_text(q.stdout+q.stderr);print(q.stdout+q.stderr,end='');q.check_returncode();(o/'receipt.json').write_text(json.dumps(dict(production_source_sha256=hashlib.sha256((S/'typed_resource.c').read_bytes()).hexdigest(),ASAN_UBSAN=True,runtime_WDK_mocked=True,hardware_verified=False),indent=2)+'\n')
