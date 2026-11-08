/* SPDX-License-Identifier: MIT. Real production UMD callbacks, mocked runtime. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-render/umd/clear_ddi.h"
#include <dxgi.h>
static UINT allocs,locks,unlocks,fail_dest,busy,free_calls;
static BYTE src[800*600*4],dst[800*600*4];
static HRESULT APIENTRY allocate(HANDLE h,D3DDDICB_ALLOCATE *q){(void)h;q->pAllocationInfo[0].hAllocation=++allocs;return S_OK;}
static HRESULT APIENTRY deallocate(HANDLE h,const D3DDDICB_DEALLOCATE *q){(void)h;(void)q;free_calls++;return S_OK;}
static HRESULT APIENTRY lock_cb(HANDLE h,D3DDDICB_LOCK *q){(void)h;locks++;assert(q->Flags.DonotWait&&!q->Flags.IgnoreSync&&!q->Flags.Discard);if(q->hAllocation==1&&busy){busy--;return D3DDDIERR_WASSTILLDRAWING;}if(q->hAllocation==2&&fail_dest)return DXGI_ERROR_DEVICE_HUNG;q->pData=q->hAllocation==1?src:dst;return S_OK;}
static HRESULT APIENTRY unlock_cb(HANDLE h,const D3DDDICB_UNLOCK *q){(void)h;assert(q->NumAllocations==1);unlocks++;return S_OK;}
int main(void){
 struct geo_umd_device d={0};D3DDDI_DEVICEFUNCS f={0};D3DDDIARG_CREATERESOURCE a={0},b;D3DDDI_SURFACEINFO s={0};D3DDDIARG_BLT q={0};D3DDDIARG_LOCK l={0};D3DDDIARG_UNLOCK u={0};
 SetEnvironmentVariableW(L"MT8167_D3D_PROGRESS_V1",NULL);d.cb.pfnAllocateCb=allocate;d.cb.pfnDeallocateCb=deallocate;d.cb.pfnLockCb=lock_cb;d.cb.pfnUnlockCb=unlock_cb;
 geo_umd_install_clear_ddis(&f);s.Width=800;s.Height=600;s.Depth=1;a.Format=D3DDDIFMT_A8R8G8B8;a.Pool=D3DDDIPOOL_VIDEOMEMORY;a.pSurfList=&s;a.SurfCount=a.MipLevels=1;a.Rotation=D3DDDI_ROTATION_IDENTITY;a.Flags.RenderTarget=1;
 assert(!f.pfnCreateResource(&d,&a));b=a;b.Flags.RenderTarget=0;b.Pool=D3DDDIPOOL_SYSTEMMEM;assert(!f.pfnCreateResource(&d,&b));
 q.hSrcResource=a.hResource;q.hDstResource=b.hResource;q.SrcRect.right=q.DstRect.right=800;q.SrcRect.bottom=q.DstRect.bottom=600;
 memset(src,0x6c,sizeof(src));busy=2;assert(!f.pfnBlt(&d,&q));assert(locks==4&&unlocks==2&&!memcmp(src,dst,sizeof(src)));
 memset(dst,0xa5,sizeof(dst));fail_dest=1;assert(f.pfnBlt(&d,&q)==DXGI_ERROR_DEVICE_HUNG);assert(unlocks==3&&!free_calls&&dst[0]==0xa5);fail_dest=0;
 l.hResource=a.hResource;l.Flags.ReadOnly=1;l.Flags.DoNotWait=1;busy=1;UINT before=locks;assert(f.pfnLock(&d,&l)==D3DDDIERR_WASSTILLDRAWING&&locks==before+1);
 assert(!f.pfnLock(&d,&l));assert(f.pfnDestroyResource(&d,a.hResource)==E_FAIL&&!free_calls);u.hResource=a.hResource;assert(!f.pfnUnlock(&d,&u));
 assert(!f.pfnDestroyResource(&d,a.hResource)&&!f.pfnDestroyResource(&d,b.hResource)&&free_calls==2);
 puts("production Lock/Blt: nonblocking flags / real busy retry / real pixels / failed destination no copy + source unlock / no free while locked PASS");
}
