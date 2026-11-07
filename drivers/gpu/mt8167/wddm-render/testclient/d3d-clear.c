/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <dxgi.h>
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
static int hex8(const WCHAR **p,UINT *v) {
 UINT i,x;*v=0;
 for(i=0;i<8;i++) {WCHAR c=*(*p)++;if(c>='0'&&c<='9')x=c-'0';else if(c>='a'&&c<='f')x=c-'a'+10;else if(c>='A'&&c<='F')x=c-'A'+10;else return 0;*v=(*v<<4)|x;}return 1;
}
static int luid_arg(LUID *id) {
 const WCHAR *p=GetCommandLineW();UINT high,low;
 if(*p=='"'){p++;while(*p&&*p!='"')p++;if(*p)p++;}else while(*p&&*p!=' ')p++;
 while(*p==' ')p++;
 if(*p++!='-'||*p++!='-'||*p++!='l'||*p++!='u'||*p++!='i'||*p++!='d'||*p++!='=')return 0;
 if(!hex8(&p,&high)||*p++!=':'||!hex8(&p,&low))return 0;
 while(*p==' ')p++;if(*p)return 0;
 id->HighPart=(LONG)high;id->LowPart=low;return 1;
}
static HRESULT probe(IDXGIAdapter *adapter) {
 ID3D11Device *device=NULL;ID3D11DeviceContext *ctx=NULL;ID3D11Texture2D *target=NULL,*staging=NULL;
 ID3D11RenderTargetView *view=NULL;D3D_FEATURE_LEVEL level,wanted=D3D_FEATURE_LEVEL_9_1;
 D3D11_TEXTURE2D_DESC desc={0};D3D11_MAPPED_SUBRESOURCE map;HRESULT hr;UINT pass,x,y,match,mismatch;
 const FLOAT colors[2][4]={{1,0,1,1},{0,1,0,1}};
 const UINT expected[2]={0xffff00ffu,0xff00ff00u};
 hr=D3D11CreateDevice(adapter,D3D_DRIVER_TYPE_UNKNOWN,NULL,0,&wanted,1,D3D11_SDK_VERSION,&device,&level,&ctx);
 line("CREATE_DEVICE_HRESULT",hr);if(FAILED(hr))goto done;
 line("FEATURE_LEVEL",level);
 desc.Width=256;desc.Height=256;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
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
  if(!map.pData||map.RowPitch<256*4) {
   ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)staging,0);hr=E_UNEXPECTED;goto done;
  }
  for(y=0;y<256;y++)for(x=0;x<256;x++) {
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
 done:
 if(view)ID3D11RenderTargetView_Release(view);
 if(staging)ID3D11Texture2D_Release(staging);
 if(target)ID3D11Texture2D_Release(target);
 if(ctx)ID3D11DeviceContext_Release(ctx);
 if(device)ID3D11Device_Release(device);
 return hr;
}
void mainCRTStartup(void) {
 IDXGIFactory *factory=NULL;IDXGIAdapter *a=NULL,*chosen=NULL;DXGI_ADAPTER_DESC desc;
 LUID id={0};UINT i,count=0,result=1;HRESULT hr;int selected=luid_arg(&id);
 WCHAR path[]=L"C:\\MT8167D3DClear\\clear-00000000-00000000.log";
 const WCHAR hex[]=L"0123456789abcdef";UINT tick=GetTickCount(),pid=GetCurrentProcessId();
 for(i=0;i<8;i++){path[24+i]=hex[(tick>>(28-i*4))&15];path[33+i]=hex[(pid>>(28-i*4))&15];}
 CreateDirectoryW(L"C:\\MT8167D3DClear",NULL);
 disk=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
 if(disk==INVALID_HANDLE_VALUE)ExitProcess(2);
 line("SCHEMA",1);line("EXPLICIT_LUID",selected);
 hr=CreateDXGIFactory(&IID_IDXGIFactory,(void **)&factory);line("FACTORY_HRESULT",hr);if(FAILED(hr)||!log_ok)goto done;
 for(i=0;;i++) {
  hr=IDXGIFactory_EnumAdapters(factory,i,&a);
  if(hr==DXGI_ERROR_NOT_FOUND)break;
  if(FAILED(hr))goto done;
  hr=IDXGIAdapter_GetDesc(a,&desc);
  if(FAILED(hr)){IDXGIAdapter_Release(a);a=NULL;goto done;}
  line("ADAPTER",i);description(desc.Description);line("LUID_HIGH",(UINT)desc.AdapterLuid.HighPart);line("LUID_LOW",desc.AdapterLuid.LowPart);
  line("VENDOR",desc.VendorId);line("DEVICE",desc.DeviceId);
  /* LUID is selected by the principal from PnP/DXGI evidence, never by index,
   * guessed PCI identity, display name substring, or first-adapter fallback. */
  if(selected&&desc.AdapterLuid.HighPart==id.HighPart&&desc.AdapterLuid.LowPart==id.LowPart){count++;if(!chosen)chosen=a;else IDXGIAdapter_Release(a);}else IDXGIAdapter_Release(a);
  a=NULL;
 }
 line("SELECTED_COUNT",count);
 if(count!=1||!log_ok){hr=E_INVALIDARG;goto done;}
 hr=probe(chosen);if(SUCCEEDED(hr)&&log_ok)result=0;
 done:
 line("FINAL_HRESULT",hr);line("API_READBACK_SUCCESS",result==0&&log_ok);
 if(chosen)IDXGIAdapter_Release(chosen);
 if(factory)IDXGIFactory_Release(factory);
 if(!log_ok||!FlushFileBuffers(disk))result=3;
 CloseHandle(disk);ExitProcess(result);
}
