/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed MT8167 PortCls capture client. No arguments, commands or raw PCM files.
 * Explicit unsupported latency is recorded separately from RX DMA/IRQ evidence. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
/* WDK 8.1 C headers use DEFINE_GUIDEX declarations for named KS GUIDs;
 * initguid.h does not instantiate them. Define exactly the nine GUIDs used
 * below from the SDK's own STATIC_* tuples, without uuid.lib or new imports.
 * This translation unit is C; brace elision initializes GUID.Data4 in order. */
#define A04_DEFINE_KS_GUID(name) const GUID name = { STATIC_##name }
A04_DEFINE_KS_GUID(KSDATAFORMAT_TYPE_AUDIO);
A04_DEFINE_KS_GUID(KSDATAFORMAT_SUBTYPE_PCM);
A04_DEFINE_KS_GUID(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX);
A04_DEFINE_KS_GUID(KSPROPSETID_Pin);
A04_DEFINE_KS_GUID(KSMEDIUMSETID_Standard);
A04_DEFINE_KS_GUID(KSINTERFACESETID_Standard);
A04_DEFINE_KS_GUID(KSPROPSETID_Connection);
A04_DEFINE_KS_GUID(KSCATEGORY_AUDIO);
A04_DEFINE_KS_GUID(KSPROPSETID_RtAudio);
#undef A04_DEFINE_KS_GUID
#include <stddef.h>
#include "../../drivers/audio/mt8167-afe/capture_status.h"
void *memcpy(void*d,const void*s,size_t n){BYTE*a=d;const BYTE*b=s;while(n--)*a++=*b++;return d;}
void *memset(void*d,int v,size_t n){BYTE*a=d;while(n--)*a++=(BYTE)v;return d;}
static HANDLE logfile=INVALID_HANDLE_VALUE;static DWORD log_error;
static char*text(char*p,const char*s){while(*s)*p++=*s++;return p;}
static char*hex(char*p,DWORD v){static const char d[]="0123456789ABCDEF";unsigned i;for(i=0;i<8;i++)*p++=d[(v>>(28-i*4))&15];return p;}
static void line(const char*tag,DWORD a,DWORD b,DWORD c){char s[128],*p=s;DWORD n,w;
 p=text(p,"MT8167CAP ");p=text(p,tag);p=text(p," a=");p=hex(p,a);p=text(p," b=");p=hex(p,b);p=text(p," c=");p=hex(p,c);*p++='\n';n=(DWORD)(p-s);
 if(!WriteFile(logfile,s,n,&w,NULL)||w!=n)log_error=ERROR_WRITE_FAULT;
 if(!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,NULL)||w!=n)log_error=ERROR_WRITE_FAULT;
}
static int prefix(const WCHAR*s,const WCHAR*p){while(*p){WCHAR a=*s++,b=*p++;if(a>=L'a'&&a<=L'z')a-=32;if(b>=L'a'&&b<=L'z')b-=32;if(a!=b)return 0;}return 1;}
static int contains(const WCHAR*s,const WCHAR*p){for(;*s;s++)if(prefix(s,p))return 1;return 0;}
static int equal_guid(const GUID*a,const GUID*b){unsigned i;for(i=0;i<sizeof(*a);i++)if(((const BYTE*)a)[i]!=((const BYTE*)b)[i])return 0;return 1;}
static HANDLE find_filter(void){
 HDEVINFO set;SP_DEVICE_INTERFACE_DATA iface;SP_DEVINFO_DATA dev;DWORD i,need,matches=0;
 static union {ULONGLONG align;BYTE bytes[4096];}detail;static WCHAR id[512],path[1024];
 HANDLE result=INVALID_HANDLE_VALUE;
 set=SetupDiGetClassDevsW(&KSCATEGORY_AUDIO,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE)return result;
 for(i=0;i<128;i++){
  ZeroMemory(&iface,sizeof(iface));iface.cbSize=sizeof(iface);
  if(!SetupDiEnumDeviceInterfaces(set,NULL,&KSCATEGORY_AUDIO,i,&iface))break;
  ZeroMemory(&dev,sizeof(dev));dev.cbSize=sizeof(dev);
  ((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
  if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,(PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes,sizeof(detail),&need,&dev))continue;
  if(CM_Get_Device_IDW(dev.DevInst,id,512,0)!=CR_SUCCESS||!prefix(id,L"MT8167\\AFE\\"))continue;
  {const WCHAR*s=((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->DevicePath;DWORD j;ULONG status=0,problem=0;CONFIGRET rc;
   if(!contains(s,L"Mt8167CaptureWaveRT"))continue;
   rc=CM_Get_DevNode_Status(&status,&problem,dev.DevInst,0);line("pnp",rc,status,problem);
   if(rc!=CR_SUCCESS||problem||!(status&DN_STARTED))continue;
   for(j=0;j<1024&&s[j];j++)path[j]=s[j];if(j==1024){matches=2;break;}path[j]=0;matches++;
  }
 }
 SetupDiDestroyDeviceInfoList(set);line("fixed-filter",matches,0,0);
 if(matches==1)result=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
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
    if(WaitForSingleObject(operation.hEvent,5000)!=WAIT_OBJECT_0)ExitProcess(ERROR_IO_TIMEOUT);
    GetOverlappedResult(handle,&operation,got,FALSE);error=ERROR_TIMEOUT;ok=FALSE;
   }else ok=GetOverlappedResult(handle,&operation,got,FALSE);if(error!=ERROR_TIMEOUT)error=ok?ERROR_SUCCESS:GetLastError();}
 }
 CloseHandle(operation.hEvent);SetLastError(error);return ok;
}
static DWORD identifiers(HANDLE filter,ULONG property,KSIDENTIFIER *chosen) {
 KSP_PIN request;union {ULONGLONG align;BYTE bytes[1024];} buffer;
 KSMULTIPLE_ITEM *items=(KSMULTIPLE_ITEM *)buffer.bytes;KSIDENTIFIER *ids;DWORD got=0,i;
 ZeroMemory(&request,sizeof(request));request.Property.Set=KSPROPSETID_Pin;
 request.Property.Id=property;request.Property.Flags=KSPROPERTY_TYPE_GET;request.PinId=0;
 if(!control(filter,IOCTL_KS_PROPERTY,&request,sizeof(request),&buffer,sizeof(buffer),&got))return GetLastError();
 if(got<sizeof(*items) || items->Size>got || items->Size<sizeof(*items) ||
    items->Count>(items->Size-sizeof(*items))/sizeof(KSIDENTIFIER))return ERROR_INVALID_DATA;
 ids=(KSIDENTIFIER *)(items+1);
 if(property==KSPROPERTY_PIN_INTERFACES) {
  for(i=0;i<items->Count;i++)if(equal_guid(&ids[i].Set,&KSINTERFACESETID_Standard) && ids[i].Id==KSINTERFACE_STANDARD_LOOPED_STREAMING){*chosen=ids[i];return 0;}
  for(i=0;i<items->Count;i++)if(equal_guid(&ids[i].Set,&KSINTERFACESETID_Standard) && ids[i].Id==KSINTERFACE_STANDARD_STREAMING){*chosen=ids[i];return 0;}
 }else for(i=0;i<items->Count;i++)if(equal_guid(&ids[i].Set,&KSMEDIUMSETID_Standard)){*chosen=ids[i];return 0;}
 return ERROR_NOT_SUPPORTED;
}
static DWORD state(HANDLE pin,KSSTATE value) {
 KSPROPERTY property;DWORD got=0;ZeroMemory(&property,sizeof(property));
 property.Set=KSPROPSETID_Connection;property.Id=KSPROPERTY_CONNECTION_STATE;property.Flags=KSPROPERTY_TYPE_SET;
 if(!control(pin,IOCTL_KS_PROPERTY,&property,sizeof(property),&value,sizeof(value),&got))return GetLastError();
 return 0;
}
static DWORD snapshot(HANDLE f,struct a04_capture_status*s){DWORD got=0;
 ZeroMemory(s,sizeof(*s));if(!control(f,A04_CAPTURE_READ_STATUS,NULL,0,s,sizeof(*s),&got))return GetLastError();
 return got==sizeof(*s)&&s->bytes==sizeof(*s)&&s->schema==1?0:ERROR_INVALID_DATA;
}
static DWORD run(void){
 HANDLE f=INVALID_HANDLE_VALUE,pin=INVALID_HANDLE_VALUE;HMODULE ks=NULL;
 DWORD result=0,got=0,i,pass,error,started=0,changes,irqFirst,irqLast,last,hash=2166136261u,nonzero=0;
 KSRTAUDIO_BUFFER_PROPERTY buffer;KSRTAUDIO_BUFFER mapping;KSPROPERTY latency;KSRTAUDIO_HWLATENCY delay;
 struct a04_capture_status s;struct {KSPIN_CONNECT connect;KSDATAFORMAT_WAVEFORMATEX format;}request;
 typedef DWORD(WINAPI*CREATE_PIN)(HANDLE,PKSPIN_CONNECT,ACCESS_MASK,PHANDLE);CREATE_PIN create;
 f=find_filter();if(f==INVALID_HANDLE_VALUE){result=ERROR_NOT_FOUND;goto done;}
 ks=LoadLibraryW(L"ksuser.dll");if(!ks){result=GetLastError();goto done;}
 create=(CREATE_PIN)GetProcAddress(ks,"KsCreatePin");if(!create){result=GetLastError();goto done;}
 ZeroMemory(&request,sizeof(request));
 error=identifiers(f,KSPROPERTY_PIN_INTERFACES,(KSIDENTIFIER*)&request.connect.Interface);
 if(!error)error=identifiers(f,KSPROPERTY_PIN_MEDIUMS,(KSIDENTIFIER*)&request.connect.Medium);
 if(error){result=error;goto done;}
 request.connect.Priority.PriorityClass=KSPRIORITY_NORMAL;request.connect.Priority.PrioritySubClass=1;
 request.format.DataFormat.FormatSize=sizeof(request.format);request.format.DataFormat.SampleSize=4;
 request.format.DataFormat.MajorFormat=KSDATAFORMAT_TYPE_AUDIO;request.format.DataFormat.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;
 request.format.DataFormat.Specifier=KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
 request.format.WaveFormatEx.wFormatTag=WAVE_FORMAT_PCM;request.format.WaveFormatEx.nChannels=2;
 request.format.WaveFormatEx.nSamplesPerSec=48000;request.format.WaveFormatEx.wBitsPerSample=16;
 request.format.WaveFormatEx.nBlockAlign=4;request.format.WaveFormatEx.nAvgBytesPerSec=192000;
 result=create(f,&request.connect,GENERIC_READ,&pin);line("create-pin",result,0,0);if(result){pin=INVALID_HANDLE_VALUE;goto done;}
 result=state(pin,KSSTATE_ACQUIRE);if(result)goto done;
 ZeroMemory(&buffer,sizeof(buffer));ZeroMemory(&mapping,sizeof(mapping));
 buffer.Property.Set=KSPROPSETID_RtAudio;buffer.Property.Id=KSPROPERTY_RTAUDIO_BUFFER;buffer.Property.Flags=KSPROPERTY_TYPE_GET;buffer.RequestedBufferSize=16384;
 if(!control(pin,IOCTL_KS_PROPERTY,&buffer,sizeof(buffer),&mapping,sizeof(mapping),&got)){result=GetLastError();goto done;}
 if(got!=sizeof(mapping)||!mapping.BufferAddress||mapping.ActualBufferSize<16384||mapping.ActualBufferSize>65536){result=ERROR_INVALID_DATA;goto done;}
 latency.Set=KSPROPSETID_RtAudio;latency.Id=KSPROPERTY_RTAUDIO_HWLATENCY;latency.Flags=KSPROPERTY_TYPE_GET;
 error=control(pin,IOCTL_KS_PROPERTY,&latency,sizeof(latency),&delay,sizeof(delay),&got)?0:GetLastError();
 line("latency-query-unqualified",error,0,0); /* not a recording success criterion */
 for(pass=0;pass<2;pass++){
  result=state(pin,KSSTATE_PAUSE);if(result)goto done;
  result=state(pin,KSSTATE_RUN);if(result)goto done;started=1;last=MAXDWORD;changes=0;irqFirst=irqLast=0;
  for(i=0;i<20;i++){
   Sleep(10);result=snapshot(f,&s);if(result)break;
   if(s.fault||!s.running||!s.leased||(s.valid&3)!=3){result=ERROR_DEVICE_NOT_AVAILABLE;break;}
   if(i==0)irqFirst=s.irq_events;irqLast=s.irq_events;
   if(last!=MAXDWORD&&s.position_bytes!=last)changes++;last=s.position_bytes;
  }
  error=state(pin,KSSTATE_STOP);started=0;if(!result)result=error;
  line("pass-dma-irq",pass,changes,irqLast-irqFirst);line("pass-result",pass,result,s.fault);
  if(!result&&(!changes||irqLast==irqFirst))result=ERROR_NOT_READY;
  if(result)goto done;
 }
 /* Read only aggregate bytes after stop; no microphone samples written to
  * disk/stdout, and no audible/acoustic quality assertion. */
 MemoryBarrier();for(i=0;i<mapping.ActualBufferSize;i++){
  BYTE b=((volatile BYTE*)mapping.BufferAddress)[i];hash=(hash^b)*16777619u;if(b)nonzero++;
 }
 line("pcm-aggregate",mapping.ActualBufferSize,hash,nonzero);
 done:
 if(pin!=INVALID_HANDLE_VALUE){if(started){error=state(pin,KSSTATE_STOP);if(!result)result=error;}CloseHandle(pin);}
 if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);if(ks)FreeLibrary(ks);return result;
}
void mainCRTStartup(void){DWORD result;HANDLE mutex;
 CreateDirectoryW(L"C:\\Windows\\System32\\MT8167CaptureProbe",NULL);
 logfile=CreateFileW(L"C:\\Windows\\System32\\MT8167CaptureProbe\\capture.log",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 if(logfile==INVALID_HANDLE_VALUE)ExitProcess(GetLastError());
 SetLastError(0);mutex=CreateMutexW(NULL,FALSE,L"MT8167CaptureProbeOnce");
 if(!mutex||GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(logfile);ExitProcess(ERROR_ALREADY_EXISTS);}
 line("begin",1,48000,2);result=run();line("end",result,log_error,0);
 if(!FlushFileBuffers(logfile)&&!log_error)log_error=GetLastError();
 CloseHandle(logfile);CloseHandle(mutex);ExitProcess(result?result:log_error);
}
