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
static void wide(const char *tag,const WCHAR *s){DWORD i=0,j,v[2];while(s[i]&&i<1024){v[0]=v[1]=0;for(j=0;j<4&&s[i+j];j++)v[j/2]|=(DWORD)s[i+j]<<(16*(j&1));line(tag,i,v[0],v[1]);i+=j;}line("text-end",i,0,0);}
static int reference(const WCHAR*s,const WCHAR*name){const WCHAR*last=s;for(;*s;s++)if(*s==L'\\')last=s+1;return !lstrcmpiW(last,name);}
static void selected_inf(HDEVINFO set,SP_DEVINFO_DATA*dev){
 /* Both OEM published names and original basenames are valid observations.
  * Do not select a static filter from an INF filename substring. */
 static const DEVPROPKEY inf={{0xa8b865dd,0x2e3d,0x4094,{0xad,0x97,0xe5,0x93,0xa7,0x0c,0x75,0xd6}},5};
 static const DEVPROPKEY version={{0xa8b865dd,0x2e3d,0x4094,{0xad,0x97,0xe5,0x93,0xa7,0x0c,0x75,0xd6}},3};
 const DEVPROPKEY*keys[2]={&inf,&version};const char*tags[2]={"selected-inf16","selected-version16"};unsigned i;
 for(i=0;i<2;i++){WCHAR value[256];DWORD bytes=0,e=0;DEVPROPTYPE type=0;ZeroMemory(value,sizeof(value));
  if(!SetupDiGetDevicePropertyW(set,dev,keys[i],&type,(BYTE*)value,sizeof(value),&bytes,0))e=GetLastError();
  else if(type!=DEVPROP_TYPE_STRING||bytes<sizeof(WCHAR)||bytes>sizeof(value)||value[bytes/sizeof(WCHAR)-1])e=ERROR_INVALID_DATA;
  line("selected-property",i,e,type);if(!e)wide(tags[i],value);
 }
}
static HANDLE find_filter(DWORD *failure){
 HDEVINFO set;SP_DEVICE_INTERFACE_DATA iface;SP_DEVINFO_DATA dev;DWORD i,need,matches=0;
 static union {ULONGLONG align;BYTE bytes[4096];}detail;static WCHAR id[512],path[1024];
 HANDLE result=INVALID_HANDLE_VALUE;
 set=SetupDiGetClassDevsW(&KSCATEGORY_TOPOLOGY,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE){*failure=GetLastError();return result;}
 for(i=0;i<128;i++){
  ZeroMemory(&iface,sizeof(iface));iface.cbSize=sizeof(iface);
  if(!SetupDiEnumDeviceInterfaces(set,NULL,&KSCATEGORY_TOPOLOGY,i,&iface))break;
  ZeroMemory(&dev,sizeof(dev));dev.cbSize=sizeof(dev);
  ((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
  if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,(PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes,sizeof(detail),&need,&dev))continue;
  if(CM_Get_Device_IDW(dev.DevInst,id,512,0)!=CR_SUCCESS||!prefix(id,L"MT8167\\AFE\\"))continue;
  {const WCHAR*s=((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->DevicePath;DWORD j;ULONG status=0,problem=0;CONFIGRET rc;
   if(!reference(s,L"Mt8167Topology"))continue;
   rc=CM_Get_DevNode_Status(&status,&problem,dev.DevInst,0);line("pnp",rc,status,problem);
   if(rc!=CR_SUCCESS||problem||!(status&DN_STARTED))continue;
   selected_inf(set,&dev);wide("interface16",s);
   for(j=0;j<1024&&s[j];j++)path[j]=s[j];if(j==1024){matches=2;break;}path[j]=0;matches++;
  }
 }
 if(i==128)matches=2;
 SetupDiDestroyDeviceInfoList(set);line("fixed-filter",matches,0,0);
 if(matches!=1){*failure=matches?ERROR_DUP_NAME:ERROR_NOT_FOUND;return result;}
 result=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
 *failure=result==INVALID_HANDLE_VALUE?GetLastError():ERROR_SUCCESS;
 line("filter-open",*failure,GENERIC_READ|GENERIC_WRITE,FILE_FLAG_OVERLAPPED);
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
 HANDLE filter=INVALID_HANDLE_VALUE;DWORD error=0,stage=1;
 struct {KSMULTIPLE_ITEM multiple;KSJACK_DESCRIPTION jack;} description;
 struct {KSMULTIPLE_ITEM multiple;KSJACK_DESCRIPTION2 jack;} capabilities;
 const WCHAR*p=GetCommandLineW();
 if(*p=='"'){p++;while(*p&&*p!='"')p++;if(*p)p++;}else while(*p&&*p!=' '&&*p!='\t')p++;
 while(*p==' '||*p=='\t')p++;
 if(lstrcmpW(p,L"--read")){error=ERROR_INVALID_PARAMETER;goto finish;}
 stage=2;filter=find_filter(&error);if(filter==INVALID_HANDLE_VALUE)goto finish;
 ZeroMemory(&description,sizeof(description));ZeroMemory(&capabilities,sizeof(capabilities));
 stage=3;error=query(filter,KSPROPERTY_JACK_DESCRIPTION,&description,sizeof(description));if(error)goto finish;
 stage=4;error=query(filter,KSPROPERTY_JACK_DESCRIPTION2,&capabilities,sizeof(capabilities));if(error)goto finish;
 if(description.jack.IsConnected!=FALSE&&description.jack.IsConnected!=TRUE){error=ERROR_INVALID_DATA;goto finish;}
 if(capabilities.jack.JackCapabilities!=JACKDESC2_PRESENCE_DETECT_CAPABILITY||capabilities.jack.DeviceStateInfo){error=ERROR_INVALID_DATA;goto finish;}
 stage=5;line("KSJACK_CONNECTED_SAMPLE",description.jack.IsConnected,capabilities.jack.JackCapabilities,2);
finish:
 if(filter!=INVALID_HANDLE_VALUE)CloseHandle(filter);line("AFE_JACK_STAGE",stage,0,0);line("ERROR",error,0,0);ExitProcess(error);
}
