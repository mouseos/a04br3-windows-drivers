/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <setupapi.h>
#include <ntddvdeo.h>
#include <winternl.h>
#include <d3dkmthk.h>
/* CCD mode-set probe: lists every display-config path, finds the GE8300
 * adapter path, and applies it active at 800x1280. This is the first
 * deliberate attempt to force RecommendFunctionalVidPn on our adapter;
 * cofunc modality is healthy (run270: 266/266 success) but nobody ever
 * asks for a functional VidPn, so no path is committed and no output
 * exists. No pixels submitted; only topology state changes. */
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
 if(lstrlenW(p)!=24)return -1;
 if(*p++!='-'||*p++!='-'||*p++!='l'||*p++!='u'||*p++!='i'||*p++!='d'||*p++!='=')return -1;
 if(!hex8(&p,&high)||*p++!=':'||!hex8(&p,&low))return -1;
 while(*p==' ')p++;if(*p)return 0;
 id->HighPart=(LONG)high;id->LowPart=low;return 1;
}
/* Standard PnP-to-adapter identity join, available on Vista/WinBlue.
 * Proven on-device by present356 runs (PNP_MATCH_COUNT=1). */
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
  if(!SetupDiGetDeviceInstanceIdW(set,&node,instance,512,NULL)){result=HRESULT_FROM_WIN32(GetLastError());goto next;}
  memset(hardware,0,sizeof(hardware));bytes=0;
  if(!SetupDiGetDeviceRegistryPropertyW(set,&node,SPDRP_HARDWAREID,&type,(BYTE *)hardware,sizeof(hardware),&bytes)){
   result=HRESULT_FROM_WIN32(GetLastError());goto next;
  }
  if(type!=REG_MULTI_SZ||bytes<4||bytes>sizeof(hardware)||(bytes&1)||hardware[(bytes>>1)-1]||hardware[(bytes>>1)-2]){result=E_UNEXPECTED;goto next;}
  for(at=0;at<(bytes>>1)&&hardware[at];at+=(DWORD)lstrlenW(hardware+at)+1){
   if(!lstrcmpiW(hardware+at,L"ACPI\\MOU0006"))match=1;
  }
  line("PNP_FIXED_HARDWARE_MATCH",match);if(!match)goto next;
  memset(service,0,sizeof(service));bytes=0;
  if(!SetupDiGetDeviceRegistryPropertyW(set,&node,SPDRP_SERVICE,&type,(BYTE *)service,sizeof(service),&bytes)){result=HRESULT_FROM_WIN32(GetLastError());goto next;}
  if(type!=REG_SZ||bytes<2||bytes>sizeof(service)||(bytes&1)||service[(bytes>>1)-1]){result=E_UNEXPECTED;goto next;}
  if(lstrcmpiW(service,L"GE8300Render")){line("PNP_RENDER_SERVICE_MATCH",0);goto next;}
  line("PNP_RENDER_SERVICE_MATCH",1);
  {
   D3DKMT_OPENADAPTERFROMDEVICENAME open={0};D3DKMT_QUERYADAPTERINFO query={0};D3DKMT_ADAPTERTYPE caps={0};D3DKMT_CLOSEADAPTER close={0};NTSTATUS status,closed;
   open.pDeviceName=detail->DevicePath;status=open_fn(&open);line("PNP_KMT_OPEN_STATUS",(UINT)status);
   if(status<0){result=E_FAIL;goto next;}
   query.hAdapter=open.hAdapter;query.Type=KMTQAITYPE_ADAPTERTYPE;query.pPrivateDriverData=&caps;query.PrivateDriverDataSize=sizeof(caps);
   status=query_fn(&query);line("PNP_KMT_TYPE_STATUS",(UINT)status);
   line("PNP_KMT_LUID_HIGH",(UINT)open.AdapterLuid.HighPart);line("PNP_KMT_LUID_LOW",open.AdapterLuid.LowPart);
   close.hAdapter=open.hAdapter;closed=close_fn(&close);line("PNP_KMT_CLOSE_STATUS",(UINT)closed);
   if(status<0||closed<0){result=E_FAIL;goto next;}
   if(!caps.RenderSupported||caps.SoftwareDevice){line("PNP_HARDWARE_RENDER_MATCH",0);goto next;}
   line("PNP_HARDWARE_RENDER_MATCH",1);count++;*wanted=open.AdapterLuid;
  }
 next:
  HeapFree(GetProcessHeap(),0,detail);detail=NULL;
 }
 line("PNP_MATCH_COUNT",count);line("PNP_ENUM_HRESULT",result);
 SetupDiDestroyDeviceInfoList(set);FreeLibrary(gdi);
 if(FAILED(result))return result;return count==1?S_OK:E_INVALIDARG;
}
static HRESULT run_modeset(LUID id) {
 UINT32 nPaths=0,nModes=0,i,geo=0;HRESULT hr;HANDLE heap=GetProcessHeap();
 DISPLAYCONFIG_PATH_INFO *paths=NULL;DISPLAYCONFIG_MODE_INFO *modes=NULL;
 LONG qdc;
 qdc=GetDisplayConfigBufferSizes(QDC_ALL_PATHS,&nPaths,&nModes);
 line("QDC_SIZES_HRESULT",(UINT)qdc);line("QDC_NUM_PATHS",nPaths);line("QDC_NUM_MODES",nModes);
 if(qdc!=ERROR_SUCCESS)return HRESULT_FROM_WIN32((DWORD)qdc);
 if(nPaths==0||nPaths>256||nModes>1024)return E_UNEXPECTED;
 paths=HeapAlloc(heap,HEAP_ZERO_MEMORY,nPaths*sizeof(*paths));
 modes=HeapAlloc(heap,HEAP_ZERO_MEMORY,nModes*sizeof(*modes));
 if(!paths||!modes){hr=E_OUTOFMEMORY;goto done;}
 qdc=QueryDisplayConfig(QDC_ALL_PATHS,&nPaths,paths,&nModes,modes,NULL);
 line("QDC_QUERY_HRESULT",(UINT)qdc);
 if(qdc!=ERROR_SUCCESS){hr=HRESULT_FROM_WIN32((DWORD)qdc);goto done;}
 line("QDC_RET_PATHS",nPaths);line("QDC_RET_MODES",nModes);
 hr=S_OK;
 for(i=0;i<nPaths;i++){
  UINT isgeo=(paths[i].sourceInfo.adapterId.HighPart==(LONG)id.HighPart&&paths[i].sourceInfo.adapterId.LowPart==id.LowPart);
  line("PATH_ADAPTER_HIGH",(UINT)paths[i].sourceInfo.adapterId.HighPart);
  line("PATH_ADAPTER_LOW",paths[i].sourceInfo.adapterId.LowPart);
  line("PATH_SOURCE_ID",paths[i].sourceInfo.id);
  line("PATH_TARGET_ID",paths[i].targetInfo.id);
  line("PATH_FLAGS",paths[i].flags);
  line("PATH_IS_GE8300",isgeo);
  if(isgeo&&!geo){
   geo=1;
   /* Apply this single path active with its queried modes. */
   paths[i].flags|=DISPLAYCONFIG_PATH_ACTIVE;
   qdc=SetDisplayConfig(1,&paths[i],nModes,modes,SDC_APPLY|SDC_USE_SUPPLIED_DISPLAY_CONFIG|SDC_ALLOW_CHANGES);
   line("SETCONFIG_HRESULT",(UINT)qdc);
   if(qdc==ERROR_SUCCESS){line("SETCONFIG_APPLIED",1);Sleep(5000);line("SETCONFIG_SETTLED",1);}
   else hr=HRESULT_FROM_WIN32((DWORD)qdc);
  }
 }
 line("GE8300_PATH_FOUND",geo);
 if(!geo)hr=HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
 done:if(paths)HeapFree(heap,0,paths);if(modes)HeapFree(heap,0,modes);return hr;
}
void mainCRTStartup(void){
 LUID id={0};HRESULT hr;WCHAR logpath[128];const WCHAR prefix[]=L"C:\\MT8167DXGIPresent\\modeset386-";const WCHAR hex[]=L"0123456789abcdef";UINT n=0;
 CreateDirectoryW(L"C:\\MT8167DXGIPresent",NULL);
 while(prefix[n]){logpath[n]=prefix[n];n++;}
 UINT pid=GetCurrentProcessId(),tick=GetTickCount();for(UINT i=0;i<8;i++)logpath[n++]=hex[(pid>>(28-4*i))&15];logpath[n++]=L'-';
 for(UINT i=0;i<8;i++)logpath[n++]=hex[(tick>>(28-4*i))&15];
 logpath[n++]=L'.';logpath[n++]=L'l';logpath[n++]=L'o';logpath[n++]=L'g';logpath[n]=0;
 disk=CreateFileW(logpath,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);if(disk==INVALID_HANDLE_VALUE)ExitProcess(2);
 line("PID",pid);line("TICK",tick);line("CLIENT_VERSION",386);
 if(selection_arg(&id)==1)line("LUID_FROM_CMDLINE",1);else{hr=hardware_luid(&id);if(FAILED(hr))goto done;}
 line("TARGET_LUID_HIGH",(UINT)id.HighPart);line("TARGET_LUID_LOW",id.LowPart);
 hr=run_modeset(id);
 done:line("RESULT_HRESULT",hr);line("RESULT",SUCCEEDED(hr)&&log_ok);CloseHandle(disk);ExitProcess(SUCCEEDED(hr)&&log_ok?0:1);
}
