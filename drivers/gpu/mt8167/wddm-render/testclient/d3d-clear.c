/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <setupapi.h>
#include <ntddvdeo.h>
#include <d3d9.h>
#include <winternl.h>
#include <d3dkmthk.h>
#include "fixed_shaders.h"
#ifdef GEO_COLOR_PAIR_CLIENT
#include "color_shaders.h"
#endif
/* Freestanding fixed-operation probe; no private device IOCTL or payload. */
static HANDLE disk;
static int log_ok=1;
static void line(const char *key,UINT value) {
 char b[128];UINT n=0,i;DWORD written;const char hex[]="0123456789abcdef";
 while(*key&&n<100)b[n++]=*key++;
 b[n++]='=';b[n++]='0';b[n++]='x';
 for(i=0;i<8;i++)b[n++]=hex[(value>>(28-i*4))&15];
 b[n++]='\r';b[n++]='\n';
 if(!WriteFile(disk,b,n,&written,NULL)||written!=n||!FlushFileBuffers(disk))log_ok=0;
}
static void description(const WCHAR *name) {
 char b[512];DWORD written;int n;
 const char prefix[]="DXGI_DESCRIPTION=";UINT i;
 for(i=0;i<sizeof(prefix)-1;i++)b[i]=prefix[i];
 n=WideCharToMultiByte(CP_UTF8,0,name,-1,b+i,(int)sizeof(b)-(int)i-2,NULL,NULL);
 if(n<=0){log_ok=0;return;}
 i+=(UINT)n-1;b[i++]='\r';b[i++]='\n';
 if(!WriteFile(disk,b,i,&written,NULL)||written!=i||!FlushFileBuffers(disk))log_ok=0;
}
static void text_line(const char *key,const WCHAR *value) {
 /* Fixed single-thread client: bounded inventory storage lives in .bss,
  * avoiding a freestanding ARM stack-probe dependency. */
 static char b[8192];UINT i=0;int n;DWORD written;
 while(*key&&i<100)b[i++]=*key++;b[i++]='=';
 n=WideCharToMultiByte(CP_UTF8,0,value,-1,b+i,(int)sizeof(b)-(int)i-2,NULL,NULL);
 if(n<=0){log_ok=0;return;}i+=(UINT)n-1;b[i++]='\r';b[i++]='\n';
 if(!WriteFile(disk,b,i,&written,NULL)||written!=i||!FlushFileBuffers(disk))log_ok=0;
}
static int hex8(const WCHAR **p,UINT *v) {
 UINT i,x;*v=0;
 for(i=0;i<8;i++) {WCHAR c=*(*p)++;if(c>='0'&&c<='9')x=c-'0';else if(c>='a'&&c<='f')x=c-'a'+10;else if(c>='A'&&c<='F')x=c-'A'+10;else return 0;*v=(*v<<4)|x;}return 1;
}
static int selection_arg(LUID *id) {
 const WCHAR *p=GetCommandLineW();UINT high,low;
 if(*p=='"'){p++;while(*p&&*p!='"')p++;if(*p)p++;}else while(*p&&*p!=' ')p++;
 while(*p==' ')p++;
 if(!*p)return 0;
 if(!lstrcmpW(p,L"--hardware=PowerVR"))return 2;
 if(lstrlenW(p)!=24)return -1; /* --luid= plus exactly 8:8 hex */
 if(*p++!='-'||*p++!='-'||*p++!='l'||*p++!='u'||*p++!='i'||*p++!='d'||*p++!='=')return -1;
 if(!hex8(&p,&high)||*p++!=':'||!hex8(&p,&low))return -1;
 while(*p==' ')p++;if(*p)return 0;
 id->HighPart=(LONG)high;id->LowPart=low;return 1;
}
/* Standard PnP-to-adapter identity join, available on Vista/WinBlue.
 * GUID_DISPLAY_DEVICE_ARRIVAL includes render-only adapters; display-view
 * interfaces and display names cannot establish this relationship.
 * No guessed PCI VendorId/DeviceId or private QueryAdapterInfo payload. */
static HRESULT hardware_luid(LUID *wanted) {
 HDEVINFO set;SP_DEVICE_INTERFACE_DATA device_interface;SP_DEVINFO_DATA node;
 PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail=NULL;DWORD index,needed,type,bytes,error;UINT count=0;HRESULT result=S_OK;
 static WCHAR hardware[4096],service[256],instance[512];HMODULE gdi=NULL;
 PFND3DKMT_OPENADAPTERFROMDEVICENAME open_fn;PFND3DKMT_QUERYADAPTERINFO query_fn;PFND3DKMT_CLOSEADAPTER close_fn;
 gdi=LoadLibraryExW(L"gdi32.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!gdi){error=GetLastError();line("AUTO_GDI_ERROR",error);return HRESULT_FROM_WIN32(error);}
 open_fn=(PFND3DKMT_OPENADAPTERFROMDEVICENAME)GetProcAddress(gdi,"D3DKMTOpenAdapterFromDeviceName");
 query_fn=(PFND3DKMT_QUERYADAPTERINFO)GetProcAddress(gdi,"D3DKMTQueryAdapterInfo");
 close_fn=(PFND3DKMT_CLOSEADAPTER)GetProcAddress(gdi,"D3DKMTCloseAdapter");
 if(!open_fn||!query_fn||!close_fn){FreeLibrary(gdi);return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);}
 set=SetupDiGetClassDevsW(&GUID_DISPLAY_DEVICE_ARRIVAL,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE){error=GetLastError();FreeLibrary(gdi);return HRESULT_FROM_WIN32(error);}
 for(index=0;;index++){
  UINT match=0;DWORD at;
  memset(&device_interface,0,sizeof(device_interface));device_interface.cbSize=sizeof(device_interface);
  if(!SetupDiEnumDeviceInterfaces(set,NULL,&GUID_DISPLAY_DEVICE_ARRIVAL,index,&device_interface)){
   error=GetLastError();if(error!=ERROR_NO_MORE_ITEMS)result=HRESULT_FROM_WIN32(error);break;
  }
  line("PNP_INTERFACE_INDEX",index);needed=0;
  if(SetupDiGetDeviceInterfaceDetailW(set,&device_interface,NULL,0,&needed,NULL)||GetLastError()!=ERROR_INSUFFICIENT_BUFFER||needed<sizeof(*detail)||needed>65536){result=E_UNEXPECTED;break;}
  detail=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,needed);if(!detail){result=E_OUTOFMEMORY;break;}
  detail->cbSize=sizeof(*detail);memset(&node,0,sizeof(node));node.cbSize=sizeof(node);
  if(!SetupDiGetDeviceInterfaceDetailW(set,&device_interface,detail,needed,NULL,&node)){result=HRESULT_FROM_WIN32(GetLastError());goto next;}
  text_line("PNP_INTERFACE",detail->DevicePath);
  if(!SetupDiGetDeviceInstanceIdW(set,&node,instance,512,NULL)){result=HRESULT_FROM_WIN32(GetLastError());goto next;}
  text_line("PNP_INSTANCE",instance);
  memset(hardware,0,sizeof(hardware));bytes=0;
  if(!SetupDiGetDeviceRegistryPropertyW(set,&node,SPDRP_HARDWAREID,&type,(BYTE *)hardware,sizeof(hardware),&bytes)){
   result=HRESULT_FROM_WIN32(GetLastError());goto next;
  }
  if(type!=REG_MULTI_SZ||bytes<4||bytes>sizeof(hardware)||bytes%2||hardware[bytes/2-1]||hardware[bytes/2-2]){result=E_UNEXPECTED;goto next;}
  for(at=0;at<bytes/2&&hardware[at];at+=(DWORD)lstrlenW(hardware+at)+1){
   text_line("PNP_HARDWARE_ID",hardware+at);if(!lstrcmpiW(hardware+at,L"ACPI\\MOU0006"))match=1;
  }
  line("PNP_FIXED_HARDWARE_MATCH",match);if(!match)goto next;
  memset(service,0,sizeof(service));bytes=0;
  if(!SetupDiGetDeviceRegistryPropertyW(set,&node,SPDRP_SERVICE,&type,(BYTE *)service,sizeof(service),&bytes)){result=HRESULT_FROM_WIN32(GetLastError());goto next;}
  if(type!=REG_SZ||bytes<2||bytes>sizeof(service)||bytes%2||service[bytes/2-1]){result=E_UNEXPECTED;goto next;}
  text_line("PNP_SERVICE",service);
  if(lstrcmpiW(service,L"GE8300Render")){line("PNP_RENDER_SERVICE_MATCH",0);goto next;}
  line("PNP_RENDER_SERVICE_MATCH",1);
  {
   D3DKMT_OPENADAPTERFROMDEVICENAME open={0};D3DKMT_QUERYADAPTERINFO query={0};D3DKMT_ADAPTERTYPE caps={0};D3DKMT_CLOSEADAPTER close={0};NTSTATUS status,closed;
   open.pDeviceName=detail->DevicePath;status=open_fn(&open);line("PNP_KMT_OPEN_STATUS",(UINT)status);
   if(status<0){result=E_FAIL;goto next;}
   query.hAdapter=open.hAdapter;query.Type=KMTQAITYPE_ADAPTERTYPE;query.pPrivateDriverData=&caps;query.PrivateDriverDataSize=sizeof(caps);
   status=query_fn(&query);line("PNP_KMT_TYPE_STATUS",(UINT)status);line("PNP_KMT_TYPE_RAW",caps.Value);
   line("PNP_KMT_LUID_HIGH",(UINT)open.AdapterLuid.HighPart);line("PNP_KMT_LUID_LOW",open.AdapterLuid.LowPart);
   close.hAdapter=open.hAdapter;closed=close_fn(&close);line("PNP_KMT_CLOSE_STATUS",(UINT)closed);
   if(status<0||closed<0){result=E_FAIL;goto next;}
   if(!caps.RenderSupported||caps.SoftwareDevice){line("PNP_HARDWARE_RENDER_MATCH",0);goto next;}
   line("PNP_HARDWARE_RENDER_MATCH",1);count++;*wanted=open.AdapterLuid;
  }
 next:
  HeapFree(GetProcessHeap(),0,detail);detail=NULL;
  /* Keep inventory after an unreadable interface; sticky failure still rejects selection. */
 }
 line("PNP_MATCH_COUNT",count);line("PNP_ENUM_HRESULT",result);
 SetupDiDestroyDeviceInfoList(set);FreeLibrary(gdi);
 if(FAILED(result))return result;return count==1?S_OK:E_INVALIDARG;
}
/* Observation only: a D3D9 ordinal cannot be manufactured from a DXGI LUID.
 * Failure of optional inventory transport never replaces the actual D3D11
 * CreateDevice result. No private escape or driver success is synthesized. */
static void runtime_inventory(IDXGIAdapter *adapter,const LUID *wanted) {
 IDirect3D9Ex *d9=NULL;IDXGIOutput *output=NULL;UINT i,matches=0,outputs=0;HRESULT hr;LUID id;
 HMODULE d9module=NULL,gdi=NULL;HRESULT (WINAPI *create9)(UINT,IDirect3D9Ex **)=NULL;D3DKMT_OPENADAPTERFROMLUID open={0};D3DKMT_QUERYADAPTERINFO query={0};D3DKMT_ADAPTERTYPE type={0};D3DKMT_CLOSEADAPTER close={0};NTSTATUS status;
 PFND3DKMT_OPENADAPTERFROMLUID open_fn;PFND3DKMT_QUERYADAPTERINFO query_fn;PFND3DKMT_CLOSEADAPTER close_fn;
 for(i=0;;i++){
  hr=IDXGIAdapter_EnumOutputs(adapter,i,&output);if(hr==DXGI_ERROR_NOT_FOUND)break;
  if(FAILED(hr)){line("DXGI_OUTPUT_ENUM_HRESULT",hr);break;}outputs++;IDXGIOutput_Release(output);output=NULL;
 }
 line("DXGI_OUTPUT_COUNT",outputs);
 d9module=LoadLibraryExW(L"d3d9.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(d9module)create9=(HRESULT (WINAPI *)(UINT,IDirect3D9Ex **))GetProcAddress(d9module,"Direct3DCreate9Ex");
 if(!create9)line("D3D9_INVENTORY_LOADER_ERROR",GetLastError());
 hr=create9?create9(D3D_SDK_VERSION,&d9):HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);line("D3D9_API_HRESULT",hr);
 if(SUCCEEDED(hr)){
  UINT count=IDirect3D9Ex_GetAdapterCount(d9);line("D3D9_ADAPTER_COUNT",count);
  for(i=0;i<count;i++){
   hr=IDirect3D9Ex_GetAdapterLUID(d9,i,&id);line("D3D9_LUID_HRESULT",hr);
   if(FAILED(hr))continue;
   line("D3D9_ORDINAL",i);line("D3D9_LUID_HIGH",(UINT)id.HighPart);line("D3D9_LUID_LOW",id.LowPart);
   if(id.HighPart==wanted->HighPart&&id.LowPart==wanted->LowPart)matches++;
  }
  IDirect3D9Ex_Release(d9);
 }
 if(d9module)FreeLibrary(d9module);
 line("D3D9_SELECTED_LUID_MATCHES",matches);
 gdi=LoadLibraryExW(L"gdi32.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!gdi){line("KMT_GDI_LOAD_ERROR",GetLastError());return;}
 open_fn=(PFND3DKMT_OPENADAPTERFROMLUID)GetProcAddress(gdi,"D3DKMTOpenAdapterFromLuid");
 query_fn=(PFND3DKMT_QUERYADAPTERINFO)GetProcAddress(gdi,"D3DKMTQueryAdapterInfo");
 close_fn=(PFND3DKMT_CLOSEADAPTER)GetProcAddress(gdi,"D3DKMTCloseAdapter");
 if(!open_fn||!query_fn||!close_fn){line("KMT_API_AVAILABLE",0);FreeLibrary(gdi);return;}
 line("KMT_API_AVAILABLE",1);open.AdapterLuid=*wanted;status=open_fn(&open);line("KMT_OPEN_STATUS",(UINT)status);
 if(status>=0){
  query.hAdapter=open.hAdapter;query.Type=KMTQAITYPE_ADAPTERTYPE;query.pPrivateDriverData=&type;query.PrivateDriverDataSize=sizeof(type);
  status=query_fn(&query);line("KMT_ADAPTER_TYPE_STATUS",(UINT)status);
  if(status>=0){line("KMT_ADAPTER_TYPE_RAW",type.Value);line("KMT_RENDER_SUPPORTED",type.RenderSupported);line("KMT_DISPLAY_SUPPORTED",type.DisplaySupported);line("KMT_SOFTWARE_DEVICE",type.SoftwareDevice);line("KMT_POST_DEVICE",type.PostDevice);}
  close.hAdapter=open.hAdapter;status=close_fn(&close);line("KMT_CLOSE_STATUS",(UINT)status);
 }
 FreeLibrary(gdi);
}
static HRESULT triangle_probe(ID3D11Device *device,ID3D11DeviceContext *ctx,ID3D11RenderTargetView *view,ID3D11Texture2D *target,ID3D11Texture2D *staging) {
 ID3D11VertexShader *vs=NULL;ID3D11PixelShader *ps=NULL;ID3D11InputLayout *layout=NULL;ID3D11Buffer *vertices=NULL,*constants=NULL;ID3D11RasterizerState *raster=NULL;
 D3D11_INPUT_ELEMENT_DESC element={"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0};
 D3D11_BUFFER_DESC b={0};D3D11_SUBRESOURCE_DATA initial={0};D3D11_RASTERIZER_DESC rs={0};D3D11_VIEWPORT viewport={0};D3D11_MAPPED_SUBRESOURCE map;
 static const FLOAT v[3][4]={{-0.8f,0.8f,0.5f,1},{0.8f,0.8f,0.5f,1},{0,-0.8f,0.5f,1}};
 static const FLOAT red[4]={1,0,0,1},black[4]={0,0,0,1};UINT stride=16,offset=0,x,y,reds=0,blacks=0,other=0,hash=2166136261u,samples=0;HRESULT hr;
 hr=ID3D11Device_CreateVertexShader(device,geo_position_vs,sizeof(geo_position_vs),NULL,&vs);line("TRIANGLE_VS_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreatePixelShader(device,geo_constant_ps,sizeof(geo_constant_ps),NULL,&ps);line("TRIANGLE_PS_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateInputLayout(device,&element,1,geo_position_vs,sizeof(geo_position_vs),&layout);line("TRIANGLE_LAYOUT_HRESULT",hr);if(FAILED(hr))goto done;
 b.ByteWidth=sizeof(v);b.Usage=D3D11_USAGE_IMMUTABLE;b.BindFlags=D3D11_BIND_VERTEX_BUFFER;initial.pSysMem=v;
 hr=ID3D11Device_CreateBuffer(device,&b,&initial,&vertices);line("TRIANGLE_VERTEX_HRESULT",hr);if(FAILED(hr))goto done;
 b.ByteWidth=sizeof(red);b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;initial.pSysMem=red;
 hr=ID3D11Device_CreateBuffer(device,&b,&initial,&constants);line("TRIANGLE_CONSTANT_HRESULT",hr);if(FAILED(hr))goto done;
 rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;
 hr=ID3D11Device_CreateRasterizerState(device,&rs,&raster);line("TRIANGLE_RASTER_HRESULT",hr);if(FAILED(hr))goto done;
 viewport.Width=800;viewport.Height=600;viewport.MaxDepth=1;
 ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&view,NULL);ID3D11DeviceContext_RSSetViewports(ctx,1,&viewport);ID3D11DeviceContext_RSSetState(ctx,raster);
 ID3D11DeviceContext_IASetInputLayout(ctx,layout);ID3D11DeviceContext_IASetPrimitiveTopology(ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&vertices,&stride,&offset);
 ID3D11DeviceContext_VSSetShader(ctx,vs,NULL,0);ID3D11DeviceContext_PSSetShader(ctx,ps,NULL,0);ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&constants);
 if(!log_ok){hr=E_FAIL;goto done;}
 ID3D11DeviceContext_ClearRenderTargetView(ctx,view,black);line("TRIANGLE_CLEAR_ISSUED",1);
 ID3D11DeviceContext_Draw(ctx,3,0);line("TRIANGLE_DRAW_ISSUED",1);
 hr=ID3D11Device_GetDeviceRemovedReason(device);line("TRIANGLE_DEVICE_REASON",hr);if(FAILED(hr)||!log_ok)goto done;
 ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource *)staging,(ID3D11Resource *)target);ID3D11DeviceContext_Flush(ctx);
 line("TRIANGLE_MAP_BEGIN",1);if(!log_ok){hr=E_FAIL;goto done;}
 hr=ID3D11DeviceContext_Map(ctx,(ID3D11Resource *)staging,0,D3D11_MAP_READ,0,&map);line("TRIANGLE_MAP_HRESULT",hr);if(FAILED(hr))goto done;
 if(!map.pData||map.RowPitch<3200){ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)staging,0);hr=E_UNEXPECTED;goto done;}
 for(y=0;y<600;y++)for(x=0;x<800;x++){
  const BYTE *p=(const BYTE *)map.pData+y*map.RowPitch+x*4;UINT pixel=p[0]|((UINT)p[1]<<8)|((UINT)p[2]<<16)|((UINT)p[3]<<24);
  if(pixel==0xffff0000u)reds++;else if(pixel==0xff000000u)blacks++;else other++;hash=(hash^pixel)*16777619u;
 }
 {const BYTE *p=(const BYTE *)map.pData+100u*map.RowPitch+160u*4u;UINT value=p[0]|((UINT)p[1]<<8)|((UINT)p[2]<<16)|((UINT)p[3]<<24);line("TRIANGLE_UPPER_INTERIOR",value);if(value!=0xffff0000u)samples++;}
 {const BYTE *p=(const BYTE *)map.pData+500u*map.RowPitch+160u*4u;UINT value=p[0]|((UINT)p[1]<<8)|((UINT)p[2]<<16)|((UINT)p[3]<<24);line("TRIANGLE_LOWER_EXTERIOR",value);if(value!=0xff000000u)samples++;}
 line("TRIANGLE_RED_PIXELS",reds);line("TRIANGLE_BLACK_PIXELS",blacks);line("TRIANGLE_OTHER_PIXELS",other);line("TRIANGLE_HASH",hash);line("TRIANGLE_SAMPLE_MISMATCH",samples);
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)staging,0);hr=(!reds||!blacks||other||samples)?E_FAIL:S_OK;
 done:
 line("TRIANGLE_HRESULT",hr);
 ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);ID3D11DeviceContext_IASetInputLayout(ctx,NULL);ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,(ID3D11Buffer *const[]){NULL},&stride,&offset);
 ID3D11DeviceContext_VSSetShader(ctx,NULL,NULL,0);ID3D11DeviceContext_PSSetShader(ctx,NULL,NULL,0);ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,(ID3D11Buffer *const[]){NULL});ID3D11DeviceContext_RSSetState(ctx,NULL);
 if(raster)ID3D11RasterizerState_Release(raster);if(constants)ID3D11Buffer_Release(constants);if(vertices)ID3D11Buffer_Release(vertices);if(layout)ID3D11InputLayout_Release(layout);if(ps)ID3D11PixelShader_Release(ps);if(vs)ID3D11VertexShader_Release(vs);return hr;
}
#ifdef GEO_COLOR_PAIR_CLIENT
static HRESULT color_probe(ID3D11Device *device,ID3D11DeviceContext *ctx,ID3D11RenderTargetView *view,ID3D11Texture2D *target,ID3D11Texture2D *staging) {
 ID3D11VertexShader *vs=NULL;ID3D11PixelShader *ps=NULL;ID3D11InputLayout *layout=NULL;ID3D11Buffer *vertices=NULL;ID3D11RasterizerState *raster=NULL;
 D3D11_INPUT_ELEMENT_DESC elements[2]={{"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}};
 static const FLOAT data[3][8]={{-.8f,.8f,.5f,1,1,0,0,1},{.8f,.8f,.5f,1,0,1,0,1},{0,-.8f,.5f,1,0,0,1,1}},black[4]={0,0,0,1};
 D3D11_BUFFER_DESC buffer={0};D3D11_SUBRESOURCE_DATA initial={0};D3D11_RASTERIZER_DESC rs={0};D3D11_VIEWPORT viewport={0};D3D11_MAPPED_SUBRESOURCE map;
 UINT stride=32,offset=0,x,y,interior=0,exterior=0,edge=0,mismatch=0,hash=2166136261u;HRESULT hr;
 hr=ID3D11Device_CreateVertexShader(device,geo_color_vs,sizeof(geo_color_vs),NULL,&vs);line("COLOR_VS_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreatePixelShader(device,geo_color_ps,sizeof(geo_color_ps),NULL,&ps);line("COLOR_PS_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateInputLayout(device,elements,2,geo_color_vs,sizeof(geo_color_vs),&layout);line("COLOR_LAYOUT_HRESULT",hr);if(FAILED(hr))goto done;
 buffer.ByteWidth=sizeof(data);buffer.Usage=D3D11_USAGE_IMMUTABLE;buffer.BindFlags=D3D11_BIND_VERTEX_BUFFER;initial.pSysMem=data;
 hr=ID3D11Device_CreateBuffer(device,&buffer,&initial,&vertices);line("COLOR_VERTEX_HRESULT",hr);if(FAILED(hr))goto done;
 rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;
 hr=ID3D11Device_CreateRasterizerState(device,&rs,&raster);line("COLOR_RASTER_HRESULT",hr);if(FAILED(hr))goto done;
 viewport.Width=800;viewport.Height=600;viewport.MaxDepth=1;
 ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&view,NULL);ID3D11DeviceContext_RSSetViewports(ctx,1,&viewport);ID3D11DeviceContext_RSSetState(ctx,raster);
 ID3D11DeviceContext_IASetInputLayout(ctx,layout);ID3D11DeviceContext_IASetPrimitiveTopology(ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&vertices,&stride,&offset);
 ID3D11DeviceContext_VSSetShader(ctx,vs,NULL,0);ID3D11DeviceContext_PSSetShader(ctx,ps,NULL,0);ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,(ID3D11Buffer *const[]){NULL});
 if(!log_ok){hr=E_FAIL;goto done;}
 ID3D11DeviceContext_ClearRenderTargetView(ctx,view,black);ID3D11DeviceContext_Draw(ctx,3,0);line("COLOR_DRAW_ISSUED",1);
 hr=ID3D11Device_GetDeviceRemovedReason(device);line("COLOR_DEVICE_REASON",hr);if(FAILED(hr))goto done;
 ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource *)staging,(ID3D11Resource *)target);ID3D11DeviceContext_Flush(ctx);
 line("COLOR_MAP_BEGIN",1);if(!log_ok){hr=E_FAIL;goto done;}
 hr=ID3D11DeviceContext_Map(ctx,(ID3D11Resource *)staging,0,D3D11_MAP_READ,0,&map);line("COLOR_MAP_HRESULT",hr);if(FAILED(hr))goto done;
 if(!map.pData||map.RowPitch<3200){ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)staging,0);hr=E_UNEXPECTED;goto done;}
 for(y=0;y<600;y++)for(x=0;x<800;x++){
  const BYTE *p=(const BYTE *)map.pData+y*map.RowPitch+4*x;UINT value=p[0]|((UINT)p[1]<<8)|((UINT)p[2]<<16)|((UINT)p[3]<<24);INT w[3],bad=0;UINT c;
  /* Pixel-center barycentrics for (80,60),(720,60),(400,540), W=1.
   * denominator3840; exclude only the narrow +/-16 edge band. */
  w[2]=8*(INT)y-476;w[1]=6*(INT)x-4*(INT)y-239;w[0]=3840-w[1]-w[2];hash=(hash^value)*16777619u;
  if(w[0]>16&&w[1]>16&&w[2]>16){
   interior++;for(c=0;c<3;c++){INT expect=(w[c]*255+1920)/3840;INT actual=p[2-c];INT delta=actual-expect;if(delta < -2||delta > 2)bad=1;}if(p[3]!=255)bad=1;
   if(bad)mismatch++;
  }else if(w[0]<-16||w[1]<-16||w[2]<-16){exterior++;if(value!=0xff000000u)mismatch++;}
  else edge++;
 }
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)staging,0);
 line("COLOR_INTERIOR",interior);line("COLOR_EXTERIOR",exterior);line("COLOR_EDGE_EXCLUDED",edge);line("COLOR_TOLERANCE_UNORM8",2);line("COLOR_MISMATCH",mismatch);line("COLOR_HASH",hash);
 hr=interior>140000&&exterior>300000&&!mismatch?S_OK:E_FAIL;
 done:
 line("COLOR_HRESULT",hr);
 ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);ID3D11DeviceContext_IASetInputLayout(ctx,NULL);ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,(ID3D11Buffer *const[]){NULL},&stride,&offset);
 ID3D11DeviceContext_VSSetShader(ctx,NULL,NULL,0);ID3D11DeviceContext_PSSetShader(ctx,NULL,NULL,0);ID3D11DeviceContext_RSSetState(ctx,NULL);
 if(raster)ID3D11RasterizerState_Release(raster);if(vertices)ID3D11Buffer_Release(vertices);if(layout)ID3D11InputLayout_Release(layout);if(ps)ID3D11PixelShader_Release(ps);if(vs)ID3D11VertexShader_Release(vs);return hr;
}
#endif
static HRESULT probe(IDXGIAdapter *adapter) {
 ID3D11Device *device=NULL;ID3D11DeviceContext *ctx=NULL;ID3D11Texture2D *target=NULL,*staging=NULL;
 ID3D11RenderTargetView *view=NULL;D3D_FEATURE_LEVEL level,wanted=D3D_FEATURE_LEVEL_9_1;
 D3D11_TEXTURE2D_DESC desc={0};D3D11_MAPPED_SUBRESOURCE map;HRESULT hr;UINT pass,x,y,match,mismatch;
 const FLOAT colors[2][4]={{1,0,1,1},{0,1,0,1}};
 const UINT expected[2]={0xffff00ffu,0xff00ff00u};
 hr=D3D11CreateDevice(adapter,D3D_DRIVER_TYPE_UNKNOWN,NULL,0,&wanted,1,D3D11_SDK_VERSION,&device,&level,&ctx);
 line("CREATE_DEVICE_HRESULT",hr);if(FAILED(hr))goto done;
 line("FEATURE_LEVEL",level);
 desc.Width=800;desc.Height=600;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
 desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
 hr=ID3D11Device_CreateTexture2D(device,&desc,NULL,&target);line("TARGET_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateRenderTargetView(device,(ID3D11Resource *)target,NULL,&view);line("VIEW_HRESULT",hr);if(FAILED(hr))goto done;
 desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 hr=ID3D11Device_CreateTexture2D(device,&desc,NULL,&staging);line("STAGING_HRESULT",hr);if(FAILED(hr))goto done;
 for(pass=0;pass<2;pass++) {
  if(!log_ok){hr=E_FAIL;goto done;}line("PASS",pass);
  ID3D11DeviceContext_ClearRenderTargetView(ctx,view,colors[pass]);
  ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource *)staging,(ID3D11Resource *)target);
  ID3D11DeviceContext_Flush(ctx);
  /* Blocking runtime Map must wait for a genuine scheduler fence. No Sleep,
   * polling private command, CPU-generated clear, or WARP fallback. */
  line("MAP_BEGIN",pass);if(!log_ok){hr=E_FAIL;goto done;}
  hr=ID3D11DeviceContext_Map(ctx,(ID3D11Resource *)staging,0,D3D11_MAP_READ,0,&map);
  line("MAP_HRESULT",hr);if(FAILED(hr))goto done;
  match=0;mismatch=0;
  if(!map.pData||map.RowPitch<800*4) {
   ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)staging,0);hr=E_UNEXPECTED;goto done;
  }
  for(y=0;y<600;y++)for(x=0;x<800;x++) {
   const BYTE *b=(const BYTE *)map.pData+y*map.RowPitch+x*4;
   UINT v=(UINT)b[0]|((UINT)b[1]<<8)|((UINT)b[2]<<16)|((UINT)b[3]<<24);
   if(v==expected[pass])match++;else mismatch++;
  }
  ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)staging,0);
  line("MATCH",match);line("MISMATCH",mismatch);
  if(mismatch){hr=E_FAIL;goto done;}
  hr=ID3D11Device_GetDeviceRemovedReason(device);line("REMOVED_REASON",hr);if(FAILED(hr))goto done;
 }
 hr=S_OK;
 hr=triangle_probe(device,ctx,view,target,staging);
#ifdef GEO_COLOR_PAIR_CLIENT
 if(SUCCEEDED(hr))hr=color_probe(device,ctx,view,target,staging);
#endif
 done:
 if(view)ID3D11RenderTargetView_Release(view);
 if(staging)ID3D11Texture2D_Release(staging);
 if(target)ID3D11Texture2D_Release(target);
 if(ctx)ID3D11DeviceContext_Release(ctx);
 if(device)ID3D11Device_Release(device);
 return hr;
}
void mainCRTStartup(void) {
 IDXGIFactory *factory=NULL;IDXGIAdapter *a=NULL,*chosen=NULL;DXGI_ADAPTER_DESC desc;DXGI_ADAPTER_DESC1 desc1;IDXGIAdapter1 *a1=NULL;
 LUID id={0};UINT i,count=0,result=1;HRESULT hr;int selected=selection_arg(&id);HRESULT auto_hr=S_FALSE;UINT eligible;
 WCHAR path[]=L"C:\\MT8167D3DClear\\clear-00000000-00000000.log";
 const WCHAR hex[]=L"0123456789abcdef";UINT tick=GetTickCount(),pid=GetCurrentProcessId();
 for(i=0;i<8;i++){path[24+i]=hex[(tick>>(28-i*4))&15];path[33+i]=hex[(pid>>(28-i*4))&15];}
 CreateDirectoryW(L"C:\\MT8167D3DClear",NULL);
 disk=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
 if(disk==INVALID_HANDLE_VALUE)ExitProcess(2);

#ifdef GEO_COLOR_PAIR_CLIENT
 line("SCHEMA",5);line("COLOR_PAIR_PROFILE",1);
#else
 line("SCHEMA",4);
#endif
 line("SELECTION_MODE",(UINT)selected);line("EXPLICIT_LUID",selected==1);
 if(selected==2){auto_hr=hardware_luid(&id);line("AUTO_IDENTITY_HRESULT",auto_hr);}
 hr=CreateDXGIFactory(&IID_IDXGIFactory,(void **)&factory);line("FACTORY_HRESULT",hr);if(FAILED(hr)||!log_ok)goto done;
 for(i=0;;i++) {
  hr=IDXGIFactory_EnumAdapters(factory,i,&a);
  if(hr==DXGI_ERROR_NOT_FOUND)break;
  if(FAILED(hr))goto done;
  hr=IDXGIAdapter_GetDesc(a,&desc);
  if(FAILED(hr)){IDXGIAdapter_Release(a);a=NULL;goto done;}
  line("ADAPTER",i);description(desc.Description);line("LUID_HIGH",(UINT)desc.AdapterLuid.HighPart);line("LUID_LOW",desc.AdapterLuid.LowPart);
  line("VENDOR",desc.VendorId);line("DEVICE",desc.DeviceId);
  eligible=selected==1||(selected==2&&SUCCEEDED(auto_hr));
  if(selected==2){
   hr=IDXGIAdapter_QueryInterface(a,&IID_IDXGIAdapter1,(void **)&a1);line("DXGI_ADAPTER1_HRESULT",hr);
   if(FAILED(hr)){IDXGIAdapter_Release(a);a=NULL;goto done;}
   hr=IDXGIAdapter1_GetDesc1(a1,&desc1);IDXGIAdapter1_Release(a1);a1=NULL;line("DXGI_DESC1_HRESULT",hr);
   if(FAILED(hr)){IDXGIAdapter_Release(a);a=NULL;goto done;}
   line("DXGI_ADAPTER_FLAGS",desc1.Flags);
   if(desc1.AdapterLuid.HighPart!=desc.AdapterLuid.HighPart||desc1.AdapterLuid.LowPart!=desc.AdapterLuid.LowPart||desc1.VendorId!=desc.VendorId||desc1.DeviceId!=desc.DeviceId){hr=E_UNEXPECTED;IDXGIAdapter_Release(a);a=NULL;goto done;}
   if(desc1.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)eligible=0;
  }
  line("DXGI_IDENTITY_MATCH",eligible&&desc.AdapterLuid.HighPart==id.HighPart&&desc.AdapterLuid.LowPart==id.LowPart);
  if(eligible&&desc.AdapterLuid.HighPart==id.HighPart&&desc.AdapterLuid.LowPart==id.LowPart){count++;if(!chosen)chosen=a;else IDXGIAdapter_Release(a);}else IDXGIAdapter_Release(a);
  a=NULL;
 }
 line("SELECTED_COUNT",count);
 if(count!=1||!log_ok){hr=E_INVALIDARG;goto done;}
 line("SELECTED_LUID_HIGH",(UINT)id.HighPart);line("SELECTED_LUID_LOW",id.LowPart);
 /* reason1=explicit LUID; reason2=PnP HWID+service -> KMT LUID -> DXGI nonsoftware. */
 line("SELECTION_REASON",(UINT)selected);
 runtime_inventory(chosen,&id);
 if(!log_ok){hr=E_FAIL;goto done;}
 hr=probe(chosen);if(SUCCEEDED(hr)&&log_ok)result=0;
 done:
 line("FINAL_HRESULT",hr);line("API_READBACK_SUCCESS",result==0&&log_ok);
 if(chosen)IDXGIAdapter_Release(chosen);
 if(factory)IDXGIFactory_Release(factory);
 if(!log_ok||!FlushFileBuffers(disk))result=3;
 CloseHandle(disk);ExitProcess(result);
}
