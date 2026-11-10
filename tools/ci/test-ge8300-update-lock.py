#!/usr/bin/env python3
"""Execute actual backend Lock/Unlock lifetime paths, no real GPU completion claim."""
from pathlib import Path
import argparse,subprocess,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();R=Path(__file__).resolve().parents[2];S=R/'drivers/gpu/mt8167/wddm-render/umd';o=a.out.resolve();o.mkdir(parents=True,exist_ok=False);raw=(S/'clear_ddi.c').read_text();actual=raw[raw.index('struct read_lock_attempt'):raw.index('static HRESULT APIENTRY blt(')]
pre=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "readback_wait.h"
typedef int32_t LONG;typedef uint32_t UINT;typedef uint32_t DWORD;typedef int32_t HRESULT;typedef void*HANDLE;
#define APIENTRY
#define S_OK 0
#define E_INVALIDARG ((HRESULT)0x80070057u)
#define E_FAIL ((HRESULT)0x80004005u)
#define E_NOTIMPL ((HRESULT)0x80004001u)
#define E_UNEXPECTED ((HRESULT)0x8000ffffu)
#define D3DDDIERR_WASSTILLDRAWING ((HRESULT)0x8876021cu)
#define DXGI_ERROR_WAS_STILL_DRAWING ((HRESULT)0x887a000au)
#define FAILED(x) ((x)<0)
#define SUCCEEDED(x) ((x)>=0)
typedef struct{HANDLE hResource;UINT SubResourceIndex;struct{UINT Value,AreaValid,ReadOnly,DoNotWait;}Flags;struct{int32_t left,top,right,bottom;}Area;void*pSurfData;UINT Pitch,SlicePitch;}D3DDDIARG_LOCK;
typedef struct{HANDLE hResource;UINT SubResourceIndex;struct{UINT Value;}Flags;}D3DDDIARG_UNLOCK;
typedef struct{UINT hAllocation;struct{UINT ReadOnly,DonotWait,LockEntire;}Flags;void*pData;}D3DDDICB_LOCK;
typedef struct{UINT NumAllocations;UINT*phAllocations;}D3DDDICB_UNLOCK;
struct geo_umd_device{HANDLE runtime;struct{HRESULT(*pfnLockCb)(HANDLE,D3DDDICB_LOCK*);HRESULT(*pfnUnlockCb)(HANDLE,D3DDDICB_UNLOCK*);}cb;};
struct geo_umd_resource{UINT locked,offered,logical_width,logical_height,allocation;struct{UINT pitch,usage;}desc;};
static struct geo_umd_resource resource;static unsigned calls,unlocks,clock_,busy;static int null_data;static HRESULT unlock_rc;static uint8_t memory[64];
static struct geo_umd_resource*find(struct geo_umd_device*d,HANDLE h){return d&&h==&resource?&resource:NULL;}
static uint32_t GetTickCount(void){return clock_;}static void Sleep(unsigned ms){clock_+=ms;}
static int geo_progress_write(UINT a,UINT b,UINT c){(void)a;(void)b;(void)c;return 1;}
static HRESULT lockcb(HANDLE h,D3DDDICB_LOCK*q){(void)h;calls++;assert(q->hAllocation==19&&q->Flags.LockEntire&&q->Flags.DonotWait&&!q->Flags.ReadOnly);if(busy){busy--;return D3DDDIERR_WASSTILLDRAWING;}q->pData=null_data?NULL:memory;return S_OK;}
static HRESULT unlockcb(HANDLE h,D3DDDICB_UNLOCK*q){(void)h;assert(q->NumAllocations==1&&*q->phAllocations==19);unlocks++;return unlock_rc;}
'''
post=r'''
int main(void){struct geo_umd_device d={0};D3DDDIARG_LOCK q={0};D3DDDIARG_UNLOCK u={0};d.cb.pfnLockCb=lockcb;d.cb.pfnUnlockCb=unlockcb;q.hResource=u.hResource=&resource;
 resource.allocation=19;resource.logical_width=7;resource.logical_height=1;resource.desc.pitch=64;busy=2;
 assert(lock_resource(&d,&q)==S_OK&&calls==3&&clock_==2&&resource.locked&&q.pSurfData==memory);assert(unlock_resource(&d,&u)==S_OK&&!resource.locked&&unlocks==1);
 null_data=1;calls=unlocks=0;assert(lock_resource(&d,&q)==E_UNEXPECTED&&calls==1&&unlocks==1&&!resource.locked);
 unlock_rc=E_FAIL;assert(lock_resource(&d,&q)==E_FAIL&&resource.locked&&unlocks==2);assert(lock_resource(&d,&q)==E_INVALIDARG);
 assert(unlock_resource(&d,&u)==E_FAIL&&resource.locked);unlock_rc=0;assert(unlock_resource(&d,&u)==S_OK&&!resource.locked);
 puts("production backend Lock: busy polling, actual cleanup of null-success, failed-unlock retention and retry refusal PASS; runtime callbacks mocked");}
'''
f=o/'production-lock.c';f.write_text(pre+actual+post);x=subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(S),str(f),'-o',str(o/'test')],capture_output=True,text=True);(o/'compile.log').write_text(x.stdout+x.stderr);x.check_returncode();x=subprocess.run([str(o/'test')],capture_output=True,text=True);(o/'test.log').write_text(x.stdout+x.stderr);print(x.stdout+x.stderr,end='');x.check_returncode();(o/'receipt.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256((S/'clear_ddi.c').read_bytes()).hexdigest(),ASAN_UBSAN=True,runtime_mocked=True,hardware_verified=False),indent=2)+'\n')
