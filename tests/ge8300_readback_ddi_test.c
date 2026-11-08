/* SPDX-License-Identifier: MIT. Production callbacks, mocked runtime transport. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-render/umd/clear_ddi.h"
#include <dxgi.h>
static UINT allocs,locks,unlocks,busy,free_calls,renders;static HRESULT render_result;
static BYTE src[800*600*4],dst[800*600*4];
static struct geo_copy_packet commands;static D3DDDI_ALLOCATIONLIST allocations[3];
static HRESULT APIENTRY allocate(HANDLE h,D3DDDICB_ALLOCATE *q){(void)h;q->pAllocationInfo[0].hAllocation=++allocs;return S_OK;}
static HRESULT APIENTRY deallocate(HANDLE h,const D3DDDICB_DEALLOCATE *q){(void)h;(void)q;free_calls++;return S_OK;}
static HRESULT APIENTRY lock_cb(HANDLE h,D3DDDICB_LOCK *q){(void)h;locks++;assert(q->Flags.LockEntire&&!q->NumPages&&!q->pPages&&q->Flags.DonotWait&&!q->Flags.IgnoreSync&&!q->Flags.Discard);if(busy){busy--;q->hAllocation=0xfedc;q->pData=(void*)(UINT_PTR)0xdeadbeef;return D3DDDIERR_WASSTILLDRAWING;}q->pData=q->hAllocation==1?src:dst;return S_OK;}
static HRESULT APIENTRY unlock_cb(HANDLE h,const D3DDDICB_UNLOCK *q){(void)h;assert(q->NumAllocations==1);unlocks++;return S_OK;}
static HRESULT APIENTRY render_cb(HANDLE h,D3DDDICB_RENDER *q){(void)h;renders++;
 assert(q->CommandLength==32&&q->NumAllocations==2);assert(commands.magic==GEO_COPY_MAGIC&&commands.size==32&&commands.version==1&&commands.destination_index==0&&commands.source_index==1&&commands.width==800&&commands.height==600&&!commands.reserved);
 assert(allocations[0].hAllocation==2&&allocations[0].WriteOperation&&allocations[1].hAllocation==1&&!allocations[1].WriteOperation);
 q->pNewCommandBuffer=&commands;q->NewCommandBufferSize=sizeof(commands);q->pNewAllocationList=allocations;q->NewAllocationListSize=3;return render_result;}
int main(void){
 struct geo_umd_device d={0};D3DDDI_DEVICEFUNCS f={0};D3DDDIARG_CREATERESOURCE a={0},b;D3DDDI_SURFACEINFO s={0};D3DDDIARG_BLT q={0};D3DDDIARG_LOCK l={0};D3DDDIARG_UNLOCK u={0};
 SetEnvironmentVariableW(L"MT8167_D3D_PROGRESS_V1",NULL);d.cb.pfnAllocateCb=allocate;d.cb.pfnDeallocateCb=deallocate;d.cb.pfnLockCb=lock_cb;d.cb.pfnUnlockCb=unlock_cb;d.cb.pfnRenderCb=render_cb;
 d.commands=&commands;d.command_bytes=sizeof(commands);d.allocations=allocations;d.allocation_count=3;
 geo_umd_install_clear_ddis(&f);s.Width=800;s.Height=600;s.Depth=1;a.Format=D3DDDIFMT_A8R8G8B8;a.Pool=D3DDDIPOOL_VIDEOMEMORY;a.pSurfList=&s;a.SurfCount=a.MipLevels=1;a.Rotation=D3DDDI_ROTATION_IDENTITY;a.Flags.RenderTarget=1;
 assert(!f.pfnCreateResource(&d,&a));b=a;b.Flags.RenderTarget=0;b.Pool=D3DDDIPOOL_SYSTEMMEM;assert(!f.pfnCreateResource(&d,&b));
 q.hSrcResource=a.hResource;q.hDstResource=b.hResource;q.SrcRect.right=q.DstRect.right=800;q.SrcRect.bottom=q.DstRect.bottom=600;
 memset(src,0x6c,sizeof(src));memset(dst,0xa5,sizeof(dst));assert(!f.pfnBlt(&d,&q));assert(renders==1&&!locks&&!unlocks&&dst[0]==0xa5);
 render_result=DXGI_ERROR_DEVICE_HUNG;assert(f.pfnBlt(&d,&q)==DXGI_ERROR_DEVICE_HUNG&&renders==2&&!locks&&!free_calls&&dst[0]==0xa5);
 l.hResource=a.hResource;l.Flags.ReadOnly=1;l.Flags.DoNotWait=1;busy=1;assert(f.pfnLock(&d,&l)==D3DDDIERR_WASSTILLDRAWING&&locks==1);
 assert(!f.pfnLock(&d,&l));assert(f.pfnDestroyResource(&d,a.hResource)==E_FAIL&&!free_calls);u.hResource=a.hResource;assert(!f.pfnUnlock(&d,&u));
 l.hResource=b.hResource;l.Flags.DoNotWait=0;busy=2;assert(!f.pfnLock(&d,&l)&&locks==5);u.hResource=b.hResource;assert(!f.pfnUnlock(&d,&u));
 assert(!f.pfnDestroyResource(&d,a.hResource)&&!f.pfnDestroyResource(&d,b.hResource)&&free_calls==2);
 puts("production Copy: two real allocation references / no RT CPU lock or copy / render failure preserved; Map lock contract PASS");return 0;
}
