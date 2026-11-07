/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
/* Fixed standard-API operation. No IOCTL, USC upload, private draw broker,
 * WARP/REF fallback, user gestures, UART requirement or arbitrary payload. */
static HANDLE disk;static int log_ok=1;
static void line(const char *key,UINT value) {
 char out[128];UINT n=0,i;DWORD written;const char hex[]="0123456789abcdef";
 while(*key&&n<100u)out[n++]=*key++;
 out[n++]='=';out[n++]='0';out[n++]='x';for(i=0;i<8u;i++)out[n++]=hex[(value>>(28u-4u*i))&15u];out[n++]='\r';out[n++]='\n';
 if(!WriteFile(disk,out,n,&written,NULL)||written!=n||!FlushFileBuffers(disk))log_ok=0;
}
static int hex8(const WCHAR **p,UINT *v) {
 UINT i,x;*v=0;for(i=0;i<8u;i++){WCHAR c=*(*p)++;if(c>='0'&&c<='9')x=c-'0';else if(c>='a'&&c<='f')x=c-'a'+10;else if(c>='A'&&c<='F')x=c-'A'+10;else return 0;*v=(*v<<4)|x;}return 1;
}
static int luid_arg(LUID *id) {
 const WCHAR *p=GetCommandLineW();UINT high,low;
 if(*p=='"'){p++;while(*p&&*p!='"')p++;if(*p)p++;}else while(*p&&*p!=' ')p++;
 while(*p==' ')p++;
 if(*p++!='-'||*p++!='-'||*p++!='l'||*p++!='u'||*p++!='i'||*p++!='d'||*p++!='=')return 0;
 if(!hex8(&p,&high)||*p++!=':'||!hex8(&p,&low))return 0;while(*p==' ')p++;if(*p)return 0;
 id->HighPart=(LONG)high;id->LowPart=low;return 1;
}
struct vertex {FLOAT x,y,z,rhw;DWORD diffuse;};
static HRESULT probe(IDirect3D9Ex *api,UINT ordinal) {
 IDirect3DDevice9Ex *device=NULL;IDirect3DSurface9 *target=NULL,*staging=NULL;HWND window=NULL;
 D3DPRESENT_PARAMETERS pp={0};D3DCAPS9 caps;D3DLOCKED_RECT map;HRESULT hr,cleanup;UINT x,y,red=0,black=0,other=0,sample_mismatch=0,hash=2166136261u;
 static const struct vertex triangle[3]={{80.0f,60.0f,0.5f,1.0f,0xffff0000u},{720.0f,60.0f,0.5f,1.0f,0xffff0000u},{400.0f,540.0f,0.5f,1.0f,0xffff0000u}};
 hr=IDirect3D9Ex_GetDeviceCaps(api,ordinal,D3DDEVTYPE_HAL,&caps);line("ADAPTER_CAPS_HRESULT",hr);if(FAILED(hr))goto done;
 line("DRIVER_VERTEX_SHADER_VERSION",caps.VertexShaderVersion);line("DRIVER_PIXEL_SHADER_VERSION",caps.PixelShaderVersion);line("SHADE_CAPS",caps.ShadeCaps);
 window=CreateWindowExW(0,L"STATIC",L"PowerVR GE8300",WS_POPUP,0,0,800,600,NULL,NULL,GetModuleHandleW(NULL),NULL);
 if(!window){DWORD error=GetLastError();line("WINDOW_ERROR",error);hr=HRESULT_FROM_WIN32(error);goto done;}
 pp.BackBufferWidth=800;pp.BackBufferHeight=600;pp.BackBufferFormat=D3DFMT_A8R8G8B8;pp.BackBufferCount=1;
 pp.SwapEffect=D3DSWAPEFFECT_COPY;pp.hDeviceWindow=window;pp.Windowed=TRUE;pp.PresentationInterval=D3DPRESENT_INTERVAL_DEFAULT;
 hr=IDirect3D9Ex_CreateDeviceEx(api,ordinal,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,NULL,&device);
 line("CREATE_DEVICE_HRESULT",hr);if(FAILED(hr)||!log_ok)goto done;
 hr=IDirect3DDevice9Ex_CreateRenderTarget(device,800,600,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,NULL);
 line("TARGET_HRESULT",hr);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_CreateOffscreenPlainSurface(device,800,600,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&staging,NULL);
 line("STAGING_HRESULT",hr);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_SetRenderTarget(device,0,target);line("SET_TARGET_HRESULT",hr);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_SetDepthStencilSurface(device,NULL);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_SetRenderState(device,D3DRS_ZENABLE,D3DZB_FALSE);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_SetRenderState(device,D3DRS_ZWRITEENABLE,FALSE);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_SetRenderState(device,D3DRS_CULLMODE,D3DCULL_NONE);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_SetRenderState(device,D3DRS_SHADEMODE,D3DSHADE_FLAT);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_SetTexture(device,0,NULL);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_SetTextureStageState(device,0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);line("STATE_HRESULT",hr);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_SetFVF(device,D3DFVF_XYZRHW|D3DFVF_DIFFUSE);line("SET_FVF_HRESULT",hr);if(FAILED(hr))goto done;
 if(!log_ok){hr=E_FAIL;goto done;}
 hr=IDirect3DDevice9Ex_Clear(device,0,NULL,D3DCLEAR_TARGET,0xff000000u,1.0f,0);line("CLEAR_HRESULT",hr);if(FAILED(hr)||!log_ok)goto done;
 hr=IDirect3DDevice9Ex_BeginScene(device);line("BEGIN_HRESULT",hr);if(FAILED(hr))goto done;
 hr=IDirect3DDevice9Ex_DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,1,triangle,sizeof(struct vertex));line("DRAW_HRESULT",hr);
 cleanup=IDirect3DDevice9Ex_EndScene(device);line("END_HRESULT",cleanup);if(SUCCEEDED(hr)&&FAILED(cleanup))hr=cleanup;if(FAILED(hr)||!log_ok)goto done;
 line("READBACK_BEGIN",1);
 hr=IDirect3DDevice9Ex_GetRenderTargetData(device,target,staging);line("READBACK_HRESULT",hr);if(FAILED(hr))goto done;
 hr=IDirect3DSurface9_LockRect(staging,&map,NULL,D3DLOCK_READONLY);line("LOCK_HRESULT",hr);if(FAILED(hr))goto done;
 if(!map.pBits||map.Pitch<3200){IDirect3DSurface9_UnlockRect(staging);hr=E_UNEXPECTED;goto done;}
 for(y=0;y<600u;y++)for(x=0;x<800u;x++){
  const BYTE *p=(const BYTE *)map.pBits+y*map.Pitch+x*4u;UINT pixel=(UINT)p[0]|((UINT)p[1]<<8)|((UINT)p[2]<<16)|((UINT)p[3]<<24);
  if(pixel==0xffff0000u)red++;else if(pixel==0xff000000u)black++;else other++;hash=(hash^pixel)*16777619u;
 }
 line("RED_PIXELS",red);line("BLACK_PIXELS",black);line("OTHER_PIXELS",other);line("PIXEL_HASH",hash);
 {const BYTE *p=(const BYTE *)map.pBits+100u*map.Pitch+160u*4u;UINT value=(UINT)p[0]|((UINT)p[1]<<8)|((UINT)p[2]<<16)|((UINT)p[3]<<24);line("UPPER_LEFT_INTERIOR",value);if(value!=0xffff0000u)sample_mismatch++;}
 {const BYTE *p=(const BYTE *)map.pBits+500u*map.Pitch+160u*4u;UINT value=(UINT)p[0]|((UINT)p[1]<<8)|((UINT)p[2]<<16)|((UINT)p[3]<<24);line("LOWER_LEFT_EXTERIOR",value);if(value!=0xff000000u)sample_mismatch++;}
 line("SAMPLE_MISMATCH",sample_mismatch);
 hr=IDirect3DSurface9_UnlockRect(staging);line("UNLOCK_HRESULT",hr);
 if(SUCCEEDED(hr)&&(!red||!black||other||sample_mismatch))hr=E_FAIL;
 done:
 line("PROBE_HRESULT",hr);
 if(staging)IDirect3DSurface9_Release(staging);if(target)IDirect3DSurface9_Release(target);if(device)IDirect3DDevice9Ex_Release(device);if(window)DestroyWindow(window);
 return hr;
}
void mainCRTStartup(void) {
 IDirect3D9Ex *api=NULL;LUID wanted={0},id;UINT count,i,selected=UINT_MAX,matches=0,result=1;HRESULT hr;int has_luid=luid_arg(&wanted);
 WCHAR path[]=L"C:\\MT8167D3D9Probe\\flat-00000000.log";const WCHAR hex[]=L"0123456789abcdef";UINT tick=GetTickCount();
 for(i=0;i<8u;i++)path[24u+i]=hex[(tick>>(28u-i*4u))&15u];
 CreateDirectoryW(L"C:\\MT8167D3D9Probe",NULL);disk=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
 if(disk==INVALID_HANDLE_VALUE)ExitProcess(2);
 line("SCHEMA",1);line("EXPLICIT_LUID",has_luid);if(!has_luid||!log_ok){hr=E_INVALIDARG;goto done;}
 hr=Direct3DCreate9Ex(D3D_SDK_VERSION,&api);line("CREATE_API_HRESULT",hr);if(FAILED(hr))goto done;
 count=IDirect3D9Ex_GetAdapterCount(api);line("ADAPTER_COUNT",count);
 for(i=0;i<count;i++){hr=IDirect3D9Ex_GetAdapterLUID(api,i,&id);line("LUID_HRESULT",hr);if(FAILED(hr))continue;
  line("ADAPTER",i);line("LUID_HIGH",(UINT)id.HighPart);line("LUID_LOW",id.LowPart);
  if(id.HighPart==wanted.HighPart&&id.LowPart==wanted.LowPart){selected=i;matches++;}}
 line("MATCHED_ADAPTERS",matches);if(matches!=1u||!log_ok){hr=E_INVALIDARG;goto done;}
 hr=probe(api,selected);if(SUCCEEDED(hr)&&log_ok)result=0;
 done:if(!log_ok)result=1;line("FINAL_HRESULT",hr);line("EXIT_CODE",result);if(!log_ok)result=1;if(api)IDirect3D9Ex_Release(api);if(!FlushFileBuffers(disk))result=1;CloseHandle(disk);ExitProcess(result);
}
