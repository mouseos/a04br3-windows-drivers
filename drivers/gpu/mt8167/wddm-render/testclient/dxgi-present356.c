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
#ifdef GEO_TEXTURE_CLIENT
#include "texture_shaders.h"
#endif
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

#include "../scanout_observe356.h"
/* Standard desktop Blt-model swapchain. The only escape below READS the
 * actual selected backing after real GPU completion and a later frame IRQ.
 * It never submits pixels or changes display/GPU state. */
static WCHAR evidence_path[128];static UINT evidence_digit;
static LRESULT CALLBACK window_proc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcW(h,m,w,l);}
static HRESULT scanout(LUID id,UINT pass){
 HMODULE gdi;PFND3DKMT_OPENADAPTERFROMLUID open_fn;PFND3DKMT_CLOSEADAPTER close_fn;PFND3DKMT_ESCAPE escape_fn;
 D3DKMT_OPENADAPTERFROMLUID op={0};D3DKMT_CLOSEADAPTER cl={0};D3DKMT_ESCAPE e={0};
 struct geo_scanout_observe356 *o=NULL;NTSTATUS st;HRESULT hr=E_FAIL;HANDLE f=INVALID_HANDLE_VALUE;DWORD written;UINT mismatch=0;
 gdi=LoadLibraryExW(L"gdi32.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!gdi)return HRESULT_FROM_WIN32(GetLastError());
 open_fn=(PFND3DKMT_OPENADAPTERFROMLUID)GetProcAddress(gdi,"D3DKMTOpenAdapterFromLuid");
 close_fn=(PFND3DKMT_CLOSEADAPTER)GetProcAddress(gdi,"D3DKMTCloseAdapter");escape_fn=(PFND3DKMT_ESCAPE)GetProcAddress(gdi,"D3DKMTEscape");
 if(!open_fn||!close_fn||!escape_fn)goto done;
 o=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*o));if(!o){hr=E_OUTOFMEMORY;goto done;}
 o->size=sizeof(*o);o->version=1;o->magic=GEO_SCANOUT_OBSERVE356_MAGIC;
 op.AdapterLuid=id;st=open_fn(&op);line("SCANOUT_OPEN_STATUS",st);if(st<0)goto done;
 e.hAdapter=op.hAdapter;e.Type=D3DKMT_ESCAPE_DRIVERPRIVATE;e.pPrivateDriverData=o;e.PrivateDriverDataSize=sizeof(*o);
 st=escape_fn(&e);line("SCANOUT_OBSERVE_STATUS",st);cl.hAdapter=op.hAdapter;line("SCANOUT_CLOSE_STATUS",close_fn(&cl));if(st<0)goto done;
 if(o->size!=sizeof(*o)||o->magic!=GEO_SCANOUT_OBSERVE356_MAGIC||o->flags!=3||o->width!=800||o->height!=1280||o->pitch!=3200||o->bytes!=GEO_SCANOUT_OBSERVE356_BYTES)goto done;
 line("SCANOUT_FRAME_LOW",o->frame_low);line("SCANOUT_FRAME_HIGH",o->frame_high);line("SCANOUT_FENCE",o->completed_fence);
 line("SCANOUT_PIXEL_HASH",o->pixel_hash);line("SCANOUT_PFN_HASH",o->pfn_hash);
 if(pass<2){UINT expected=pass?0xff00ff00u:0xffff00ffu;
  for(UINT i=0;i<o->bytes;i+=4){UINT value=o->pixels[i]|((UINT)o->pixels[i+1]<<8)|((UINT)o->pixels[i+2]<<16)|((UINT)o->pixels[i+3]<<24);if(value!=expected)mismatch++;}
  line("SCANOUT_PIXEL_MISMATCH",mismatch);if(mismatch)goto done;
 }
 evidence_path[evidence_digit]=(WCHAR)(L'0'+pass);
 f=CreateFileW(evidence_path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
 if(f==INVALID_HANDLE_VALUE){hr=HRESULT_FROM_WIN32(GetLastError());goto done;}
 if(!WriteFile(f,o,sizeof(*o),&written,NULL)||written!=sizeof(*o)||!FlushFileBuffers(f))goto done;
 hr=S_OK;
 done:if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);if(o)HeapFree(GetProcessHeap(),0,o);FreeLibrary(gdi);return hr;
}
static HRESULT run_swapchain(IDXGIFactory *factory,IDXGIAdapter *adapter,LUID id){
 ID3D11Device *dev=NULL;ID3D11DeviceContext *ctx=NULL;IDXGISwapChain *swap=NULL;IDXGIOutput *output=NULL;
 ID3D11Texture2D *back=NULL;ID3D11RenderTargetView *view=NULL;D3D_FEATURE_LEVEL fl,wanted=D3D_FEATURE_LEVEL_10_0;
 WNDCLASSW wc={0};HWND window=NULL;DXGI_SWAP_CHAIN_DESC q={0};HRESULT hr;MSG msg;UINT pass;
 const FLOAT colors[2][4]={{1,0,1,1},{0,1,0,1}};
 wc.lpfnWndProc=window_proc;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"MT8167RealPresent356";
 if(!RegisterClassW(&wc)){hr=HRESULT_FROM_WIN32(GetLastError());goto done;}
 window=CreateWindowExW(0,wc.lpszClassName,L"MT8167 GPU Present356",WS_POPUP,0,0,800,1280,NULL,NULL,wc.hInstance,NULL);
 if(!window){hr=HRESULT_FROM_WIN32(GetLastError());goto done;}
 ShowWindow(window,SW_SHOW);UpdateWindow(window);
 hr=D3D11CreateDevice(adapter,D3D_DRIVER_TYPE_UNKNOWN,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,&wanted,1,D3D11_SDK_VERSION,&dev,&fl,&ctx);
 line("CREATE_DEVICE_HRESULT",hr);if(FAILED(hr)||fl!=wanted)goto done;
 hr=IDXGIAdapter_EnumOutputs(adapter,0,&output);line("ENUM_SAME_ADAPTER_OUTPUT_HRESULT",hr);if(FAILED(hr))goto done;
 q.BufferDesc.Width=800;q.BufferDesc.Height=1280;q.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
 q.SampleDesc.Count=1;q.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;q.BufferCount=1;q.OutputWindow=window;q.Windowed=TRUE;
 q.SwapEffect=DXGI_SWAP_EFFECT_DISCARD; /* no unimplemented flip-model cap */
 hr=IDXGIFactory_CreateSwapChain(factory,(IUnknown *)dev,&q,&swap);line("CREATE_SWAPCHAIN_HRESULT",hr);if(FAILED(hr))goto done;
 hr=IDXGISwapChain_SetFullscreenState(swap,TRUE,output);line("FULLSCREEN_HRESULT",hr);if(FAILED(hr))goto done;
 hr=IDXGISwapChain_GetBuffer(swap,0,&IID_ID3D11Texture2D,(void **)&back);line("SWAPCHAIN_BUFFER_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource *)back,NULL,&view);line("SWAPCHAIN_VIEW_HRESULT",hr);if(FAILED(hr))goto done;
 for(pass=0;pass<2;pass++){
  while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
  line("FRAME_BEGIN",pass);ID3D11DeviceContext_ClearRenderTargetView(ctx,view,colors[pass]);
  line("STANDARD_PRESENT_BEFORE",pass);hr=IDXGISwapChain_Present(swap,0,0);line("STANDARD_PRESENT_HRESULT",hr);if(hr!=S_OK)goto done;
  hr=scanout(id,pass);line("REAL_SCANOUT_OBSERVATION_HRESULT",hr);if(FAILED(hr))goto done;
 }
 hr=ID3D11Device_GetDeviceRemovedReason(dev);line("DEVICE_REMOVED_REASON",hr);
 done:if(view)ID3D11RenderTargetView_Release(view);if(back)ID3D11Texture2D_Release(back);
 if(swap){HRESULT restored=IDXGISwapChain_SetFullscreenState(swap,FALSE,NULL);line("LEAVE_FULLSCREEN_HRESULT",restored);if(SUCCEEDED(hr)&&FAILED(restored))hr=restored;IDXGISwapChain_Release(swap);}
 if(output)IDXGIOutput_Release(output);if(ctx)ID3D11DeviceContext_Release(ctx);if(dev)ID3D11Device_Release(dev);
 if(window)DestroyWindow(window);UnregisterClassW(wc.lpszClassName,wc.hInstance);return hr;
}
void mainCRTStartup(void){
 IDXGIFactory *factory=NULL;IDXGIAdapter *chosen=NULL;LUID id={0};HRESULT hr;UINT matches=0;
 WCHAR logpath[128];const WCHAR prefix[]=L"C:\\MT8167DXGIPresent\\present356-";const WCHAR hex[]=L"0123456789abcdef";UINT n=0;
 CreateDirectoryW(L"C:\\MT8167DXGIPresent",NULL);
 while(prefix[n]){logpath[n]=prefix[n];n++;}
 UINT pid=GetCurrentProcessId(),tick=GetTickCount();for(UINT i=0;i<8;i++)logpath[n++]=hex[(pid>>(28-4*i))&15];logpath[n++]=L'-';
 for(UINT i=0;i<8;i++)logpath[n++]=hex[(tick>>(28-4*i))&15];
 for(UINT i=0;i<n;i++)evidence_path[i]=logpath[i];UINT base=n;evidence_path[n++]=L'-';evidence_digit=n;evidence_path[n++]=L'0';evidence_path[n++]=L'.';evidence_path[n++]=L'b';evidence_path[n++]=L'i';evidence_path[n++]=L'n';evidence_path[n]=0;
 /* Keep one exact frame digit index; no command line supplies an output path. */
 logpath[base++]=L'.';logpath[base++]=L'l';logpath[base++]=L'o';logpath[base++]=L'g';logpath[base]=0;
 disk=CreateFileW(logpath,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);if(disk==INVALID_HANDLE_VALUE)ExitProcess(2);
 line("PID",pid);line("TICK",tick);line("CLIENT_VERSION",356);hr=hardware_luid(&id);if(FAILED(hr))goto done;
 hr=CreateDXGIFactory(&IID_IDXGIFactory,(void **)&factory);if(FAILED(hr))goto done;
 for(UINT i=0;;i++){IDXGIAdapter *a=NULL;DXGI_ADAPTER_DESC desc;
  hr=IDXGIFactory_EnumAdapters(factory,i,&a);if(hr==DXGI_ERROR_NOT_FOUND){hr=S_OK;break;}if(FAILED(hr))goto done;
  hr=IDXGIAdapter_GetDesc(a,&desc);if(FAILED(hr)){IDXGIAdapter_Release(a);goto done;}
  if(desc.AdapterLuid.HighPart==id.HighPart&&desc.AdapterLuid.LowPart==id.LowPart){matches++;if(!chosen)chosen=a;else IDXGIAdapter_Release(a);}else IDXGIAdapter_Release(a);
 }
 if(matches!=1){hr=E_UNEXPECTED;goto done;}
 hr=run_swapchain(factory,chosen,id);
 done:line("RESULT_HRESULT",hr);line("RESULT",SUCCEEDED(hr)&&log_ok);if(chosen)IDXGIAdapter_Release(chosen);if(factory)IDXGIFactory_Release(factory);CloseHandle(disk);ExitProcess(SUCCEEDED(hr)&&log_ok?0:1);
}
