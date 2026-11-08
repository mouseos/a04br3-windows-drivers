/* SPDX-License-Identifier: MIT. Production SRV/sampler lifetime regression.
 * Resource lookup/allocator and fence callbacks are mocks; no GPU is opened. */
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include "../drivers/gpu/mt8167/wddm-render/umd/typed_device.h"
#include "../drivers/gpu/mt8167/wddm-render/umd/typed_state.h"
#include "../drivers/gpu/mt8167/wddm-render/frontend/texture_draw_abi.h"
_Static_assert(offsetof(struct geo_draw_packet_v3,request)==offsetof(struct geo_draw_packet,request),"versioned prefix");
static struct geo_typed_device device;static struct geo_typed_resource resource;static HRESULT error;static int releases;
struct geo_typed_device *geo_typed_device(D3D10DDI_HDEVICE h){return h.pDrvPrivate==&device?&device:NULL;}
struct geo_typed_state *geo_typed_state_from_device(D3D10DDI_HDEVICE h){return geo_typed_device(h)?device.state:NULL;}
void geo_typed_error(D3D10DDI_HDEVICE h,HRESULT hr){(void)h;error=hr;}
struct geo_typed_resource *geo_typed_resource(D3D10DDI_HDEVICE h,D3D10DDI_HRESOURCE r){return geo_typed_device(h)&&r.pDrvPrivate==&resource&&!resource.deleted?&resource:NULL;}
void geo_typed_resource_ref(struct geo_typed_resource *r){assert(r==&resource);r->refs++;}
HRESULT geo_typed_resource_unref(struct geo_typed_resource *r){assert(r==&resource&&r->refs);r->refs--;releases++;return S_OK;}
HRESULT geo_typed_wait_resource(struct geo_typed_resource *r){return r==&resource?S_OK:E_INVALIDARG;}
HRESULT geo_umd_resource_info(struct geo_umd_device *d,HANDLE h,UINT usage,struct geo_surface_desc *out,D3DKMT_HANDLE *a){assert(d==&device.backend&&h==&resource&&usage==GEO_USAGE_TEXTURE2D);*out=(struct geo_surface_desc){sizeof(*out),2,4,4,16,GEO_FORMAT_A8R8G8B8,GEO_USAGE_TEXTURE2D,0};*a=77;return S_OK;}
int main(void){
 D3D10DDI_HDEVICE h={&device};D3D10DDI_HRESOURCE r={&resource};D3D11DDIARG_CREATESHADERRESOURCEVIEW desc={0};D3D10DDI_HSHADERRESOURCEVIEW srv={0},nil={0};D3D10DDI_HRTSHADERRESOURCEVIEW rt={0};D3D10DDI_HSAMPLER sampler={0};D3D10DDI_HRTSAMPLER srt={0};D3D10_DDI_SAMPLER_DESC sd={0};D3DKMT_HANDLE allocation;struct geo_surface_desc storage;
 assert(SUCCEEDED(geo_typed_state_create(&device.state)));geo_typed_state_install(&device.functions);geo_typed_texture_install(&device.functions);
 resource.refs=1;resource.device=&device;resource.backend=&resource;resource.bind=D3D10_DDI_BIND_SHADER_RESOURCE;resource.dimension=D3D10DDIRESOURCE_TEXTURE2D;resource.format=DXGI_FORMAT_B8G8R8A8_UNORM;
 desc.hDrvResource=r;desc.Format=resource.format;desc.ResourceDimension=resource.dimension;desc.Tex2D.MipLevels=desc.Tex2D.ArraySize=1;
 SIZE_T bytes=device.functions.pfnCalcPrivateShaderResourceViewSize(h,&desc);assert(bytes==sizeof(struct geo_typed_handle));srv.pDrvPrivate=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,bytes);
 device.functions.pfnCreateShaderResourceView(h,&desc,srv,rt);assert(!error&&resource.refs==2);
 device.functions.pfnPsSetShaderResources(h,0,1,&srv);assert(device.texture);
 assert(FAILED(geo_typed_texture_allocation(&device,&storage,&allocation))); /* NULL sampler is LINEAR */
 sd.Filter=D3D10_DDI_FILTER_MIN_MAG_MIP_POINT;sd.AddressU=sd.AddressV=sd.AddressW=D3D10_DDI_TEXTURE_ADDRESS_CLAMP;sd.MinLOD=-3.0f;sd.MaxLOD=3.0f;
 bytes=device.functions.pfnCalcPrivateSamplerSize(h,&sd);sampler.pDrvPrivate=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,bytes);assert(sampler.pDrvPrivate);
 device.functions.pfnCreateSampler(h,&sd,sampler,srt);assert(!error);device.functions.pfnPsSetSamplers(h,0,1,&sampler);assert(!error);
 assert(SUCCEEDED(geo_typed_texture_allocation(&device,&storage,&allocation))&&allocation==77&&storage.version==2);
 device.functions.pfnDestroySampler(h,sampler);HeapFree(GetProcessHeap(),0,sampler.pDrvPrivate); /* bound immutable copy survives */
 assert(SUCCEEDED(geo_typed_texture_allocation(&device,&storage,&allocation)));
 device.functions.pfnPsSetShaderResources(h,1,1,&srv);assert(error==E_NOTIMPL&&device.texture);error=S_OK;
 device.functions.pfnPsSetShaderResources(h,0,0,NULL);assert(device.texture);
 device.functions.pfnDestroyShaderResourceView(h,srv);assert(!error&&resource.refs==2&&device.texture);
 HeapFree(GetProcessHeap(),0,srv.pDrvPrivate);assert(SUCCEEDED(geo_typed_texture_allocation(&device,&storage,&allocation)));
 device.functions.pfnPsSetShaderResources(h,0,1,&nil);assert(!device.texture&&!device.texture_views&&resource.refs==1&&releases==1);
 assert(FAILED(geo_typed_texture_allocation(&device,&storage,&allocation)));
 desc.Tex2D.MipLevels=2;assert(!device.functions.pfnCalcPrivateShaderResourceViewSize(h,&desc)&&error==E_NOTIMPL);
 geo_typed_state_delete(device.state);puts("production SRV + sampler snapshot: reject null/nonzero-slot/multi-mip, preserve zero-count and destroyed-bound lifetimes");return 0;
}
