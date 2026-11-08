/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed standard KS jack GETs. No streaming, SET, raw MMIO or repeated probe. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#define A04_DEFINE_KS_GUID(name) const GUID name = { STATIC_##name }
A04_DEFINE_KS_GUID(KSCATEGORY_TOPOLOGY);
A04_DEFINE_KS_GUID(KSPROPSETID_Jack);
#undef A04_DEFINE_KS_GUID
void *memcpy(void*d,const void*s,size_t n){BYTE*a=d;const BYTE*b=s;while(n--)*a++=*b++;return d;}
void *memset(void*d,int v,size_t n){BYTE*a=d;while(n--)*a++=(BYTE)v;return d;}
static void line(const char*tag,DWORD a,DWORD b,DWORD c){
 const char *digits="0123456789ABCDEF";char text[160];DWORD n=0,w;unsigned j;DWORD values[3]={a,b,c};
 while(*tag&&n<100)text[n++]=*tag++;
 for(j=0;j<3;j++){unsigned i;text[n++]=' ';for(i=0;i<8;i++)text[n++]=digits[(values[j]>>(28-i*4))&15];}
 text[n++]='\n';WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,n,&w,NULL);
}
static int prefix(const WCHAR*s,const WCHAR*p){while(*p){WCHAR a=*s++,b=*p++;if(a>=L'a'&&a<=L'z')a-=32;if(b>=L'a'&&b<=L'z')b-=32;if(a!=b)return 0;}return 1;}
static int contains(const WCHAR*s,const WCHAR*p){for(;*s;s++)if(prefix(s,p))return 1;return 0;}
static int equal_guid(const GUID*a,const GUID*b){unsigned i;for(i=0;i<sizeof(*a);i++)if(((const BYTE*)a)[i]!=((const BYTE*)b)[i])return 0;return 1;}
static HANDLE find_filter(DWORD *error){
 HDEVINFO set;SP_DEVICE_INTERFACE_DATA iface;SP_DEVINFO_DATA dev;DWORD i,need,matches=0;
 static union {ULONGLONG align;BYTE bytes[4096];}detail;static WCHAR id[512],path[1024];
 HANDLE result=INVALID_HANDLE_VALUE;*error=ERROR_NOT_FOUND;
 set=SetupDiGetClassDevsW(&KSCATEGORY_TOPOLOGY,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE){*error=GetLastError();line("interface-set",*error,0,0);return result;}
 for(i=0;i<128;i++){
  ZeroMemory(&iface,sizeof(iface));iface.cbSize=sizeof(iface);
  if(!SetupDiEnumDeviceInterfaces(set,NULL,&KSCATEGORY_TOPOLOGY,i,&iface))break;
  ZeroMemory(&dev,sizeof(dev));dev.cbSize=sizeof(dev);
  ((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
  if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,(PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes,sizeof(detail),&need,&dev))continue;
  if(CM_Get_Device_IDW(dev.DevInst,id,512,0)!=CR_SUCCESS||!prefix(id,L"MT8167\\AFE\\"))continue;
  {const WCHAR*s=((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->DevicePath;DWORD j;ULONG status=0,problem=0;CONFIGRET rc;
   if(!contains(s,L"Mt8167Topology"))continue;
   rc=CM_Get_DevNode_Status(&status,&problem,dev.DevInst,0);line("pnp",rc,status,problem);
   if(rc!=CR_SUCCESS||problem||!(status&DN_STARTED))continue;
   for(j=0;j<1024&&s[j];j++)path[j]=s[j];if(j==1024){matches=2;break;}path[j]=0;matches++;
  }
 }
 if(i==128)matches=2;
 SetupDiDestroyDeviceInfoList(set);line("fixed-filter",matches,0,0);
 if(matches==1){
  result=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
  *error=result==INVALID_HANDLE_VALUE?GetLastError():ERROR_SUCCESS;
  line("filter-open",*error,GENERIC_READ|GENERIC_WRITE,FILE_FLAG_OVERLAPPED);
 }else if(matches>1)*error=ERROR_DUP_NAME;
 return result;
}
static BOOL control(HANDLE handle,DWORD code,void *in,DWORD inbytes,void *out,DWORD outbytes,DWORD *got) {
 OVERLAPPED operation;BOOL ok;DWORD error=ERROR_SUCCESS;
 ZeroMemory(&operation,sizeof(operation));operation.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
 if(!operation.hEvent)return FALSE;
 ok=DeviceIoControl(handle,code,in,inbytes,out,outbytes,got,&operation);
 if(!ok) {
  error=GetLastError();
  if(error==ERROR_IO_PENDING){if(WaitForSingleObject(operation.hEvent,5000)!=WAIT_OBJECT_0){
    CancelIoEx(handle,&operation);
    if(WaitForSingleObject(operation.hEvent,5000)!=WAIT_OBJECT_0)ExitProcess(ERROR_TIMEOUT);
    GetOverlappedResult(handle,&operation,got,FALSE);error=ERROR_TIMEOUT;ok=FALSE;
   }else ok=GetOverlappedResult(handle,&operation,got,FALSE);if(error!=ERROR_TIMEOUT)error=ok?ERROR_SUCCESS:GetLastError();}
 }
 CloseHandle(operation.hEvent);SetLastError(error);return ok;
}

static DWORD query(HANDLE filter,ULONG id,void *output,DWORD bytes){
 KSP_PIN request;DWORD got=0;ZeroMemory(&request,sizeof(request));
 request.Property.Set=KSPROPSETID_Jack;request.Property.Id=id;
 request.Property.Flags=KSPROPERTY_TYPE_GET;request.PinId=2;
 if(!control(filter,IOCTL_KS_PROPERTY,&request,sizeof(request),output,bytes,&got))return GetLastError();
 if(got!=bytes||((KSMULTIPLE_ITEM*)output)->Size!=bytes||((KSMULTIPLE_ITEM*)output)->Count!=1)return ERROR_INVALID_DATA;
 return ERROR_SUCCESS;
}
void mainCRTStartup(void){
 HANDLE filter=INVALID_HANDLE_VALUE;DWORD error=0;
 struct {KSMULTIPLE_ITEM multiple;KSJACK_DESCRIPTION jack;} description;
 struct {KSMULTIPLE_ITEM multiple;KSJACK_DESCRIPTION2 jack;} capabilities;
 const WCHAR*p=GetCommandLineW();
 if(*p=='"'){p++;while(*p&&*p!='"')p++;if(*p)p++;}else while(*p&&*p!=' '&&*p!='\t')p++;
 while(*p==' '||*p=='\t')p++;
 if(lstrcmpW(p,L"--read")){error=ERROR_INVALID_PARAMETER;goto finish;}
 filter=find_filter(&error);if(filter==INVALID_HANDLE_VALUE)goto finish;
 ZeroMemory(&description,sizeof(description));ZeroMemory(&capabilities,sizeof(capabilities));
 error=query(filter,KSPROPERTY_JACK_DESCRIPTION,&description,sizeof(description));if(error)goto finish;
 error=query(filter,KSPROPERTY_JACK_DESCRIPTION2,&capabilities,sizeof(capabilities));if(error)goto finish;
 if(description.jack.IsConnected!=FALSE&&description.jack.IsConnected!=TRUE){error=ERROR_INVALID_DATA;goto finish;}
 if(capabilities.jack.JackCapabilities!=JACKDESC2_PRESENCE_DETECT_CAPABILITY||capabilities.jack.DeviceStateInfo){error=ERROR_INVALID_DATA;goto finish;}
 line("KSJACK_CONNECTED_SAMPLE",description.jack.IsConnected,capabilities.jack.JackCapabilities,2);
finish:
 if(filter!=INVALID_HANDLE_VALUE)CloseHandle(filter);line("ERROR",error,0,0);ExitProcess(error);
}
