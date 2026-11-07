/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Direct KS/PortCls first RUN, without AudioSrv/MMDevice/WASAPI. No UART MMIO.
 * DMA is the driver's private immutable PCM, not this client-visible buffer. */
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
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
#include "../../drivers/audio/mt8167-afe/diagnostic.h"
void *memcpy(void *d,const void *s,size_t n) {volatile BYTE *a=d;const volatile BYTE *b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int v,size_t n) {volatile BYTE *a=d;while(n--)*a++=(BYTE)v;return d;}
typedef DWORD (WINAPI *CREATE_KS_PIN)(HANDLE,PKSPIN_CONNECT,ACCESS_MASK,PHANDLE);
static HANDLE uart=INVALID_HANDLE_VALUE,logfile=INVALID_HANDLE_VALUE;
static DWORD runid;static int uart_lost,log_lost;
static char *text(char *p,const char *s){while(*s)*p++=*s++;return p;}
static char *hex(char *p,DWORD n){static const char digits[]="0123456789ABCDEF";unsigned i;for(i=0;i<8;i++)*p++=digits[(n>>(28-4*i))&15];return p;}
static int equal_guid(const GUID *a,const GUID *b){const BYTE *x=(const BYTE *)a,*y=(const BYTE *)b;unsigned i;for(i=0;i<sizeof(*a);i++)if(x[i]!=y[i])return 0;return 1;}
static int prefix(const WCHAR *s,const WCHAR *p){while(*p){WCHAR a=*s++,b=*p++;if(a>=L'a'&&a<=L'z')a-=32;if(b>=L'a'&&b<=L'z')b-=32;if(a!=b)return 0;}return 1;}
static void emit(const char *s,DWORD n) {
 DWORD written,start=GetTickCount();
 WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&written,NULL);
 if(logfile!=INVALID_HANDLE_VALUE && (!WriteFile(logfile,s,n,&written,NULL)||written!=n))log_lost=1;
 if(uart_lost)return;
 if(n>96 || uart==INVALID_HANDLE_VALUE){uart_lost=1;return;}
 while(!DeviceIoControl(uart,A04_UART_SCREEN_SEND,(void *)s,n,NULL,0,&written,NULL)) {
  if(GetLastError()!=ERROR_BUSY || GetTickCount()-start>=2000){uart_lost=1;return;}Sleep(10);
 }
 /* The last accepted record is still asynchronous. Host END reception is
  * the delivery evidence, not DeviceIoControl success. */
}
static void line(const char *tag,DWORD a,DWORD b,DWORD c) {
 char s[96],*p=s;p=text(p,"A04AUDKS r=");p=hex(p,runid);p=text(p," ");p=text(p,tag);
 p=text(p," a=");p=hex(p,a);p=text(p," b=");p=hex(p,b);p=text(p," c=");p=hex(p,c);*p++='\n';emit(s,(DWORD)(p-s));
}
static void open_log(void) {
 WCHAR path[MAX_PATH];DWORD n=GetModuleFileNameW(NULL,path,MAX_PATH),i;char digits[8];WCHAR *p;
 if(!n || n>=MAX_PATH){log_lost=1;return;}
 while(n && path[n-1]!=L'\\' && path[n-1]!=L'/')n--;
 if(n+40>=MAX_PATH){log_lost=1;return;}
 p=path+n;{const WCHAR *s=L"audio-first-playback-";while(*s)*p++=*s++;}
 hex(digits,runid);for(i=0;i<8;i++)*p++=(WCHAR)digits[i];
 {const WCHAR *s=L".log";while(*s)*p++=*s++;}*p=0;
 logfile=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 if(logfile==INVALID_HANDLE_VALUE)log_lost=1;
}
static int open_uart(void) {
 DWORD start=GetTickCount();
 do {
  uart=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
  if(uart!=INVALID_HANDLE_VALUE)return 1;Sleep(100);
 }while(GetTickCount()-start<5000);
 return 0;
}
static HANDLE find_filter(void) {
 HDEVINFO set;SP_DEVICE_INTERFACE_DATA iface;SP_DEVINFO_DATA dev;DWORD index,need,matches=0;
 static union {ULONGLONG align;BYTE bytes[2048];} detail;
 static WCHAR id[512],path[1024];ULONG status,problem;HANDLE result=INVALID_HANDLE_VALUE;
 set=SetupDiGetClassDevsW(&KSCATEGORY_AUDIO,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE){line("enum-error",GetLastError(),0,0);return result;}
 for(index=0;;index++) {
  ZeroMemory(&iface,sizeof(iface));iface.cbSize=sizeof(iface);
  if(!SetupDiEnumDeviceInterfaces(set,NULL,&KSCATEGORY_AUDIO,index,&iface)) {
   if(GetLastError()!=ERROR_NO_MORE_ITEMS)line("enum-error",GetLastError(),index,0);break;
  }
  ZeroMemory(&detail,sizeof(detail));ZeroMemory(&dev,sizeof(dev));dev.cbSize=sizeof(dev);
  ((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
  if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,(PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes,sizeof(detail),&need,&dev))continue;
  if(CM_Get_Device_IDW(dev.DevInst,id,512,0)!=CR_SUCCESS || !prefix(id,L"MT8167\\AFE\\"))continue;
  matches++;status=problem=0;
  {CONFIGRET cr=CM_Get_DevNode_Status(&status,&problem,dev.DevInst,0);line("pnp",cr,status,problem);}
  {const WCHAR *from=((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->DevicePath;DWORD i=0;
   while(from[i] && i<1023){path[i]=from[i];i++;}path[i]=0;if(from[i])matches=2;}
 }
 SetupDiDestroyDeviceInfoList(set);line("matches",matches,0,0);
 if(matches==1) {
  result=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
  if(result==INVALID_HANDLE_VALUE)line("filter-open-error",GetLastError(),0,0);
 }
 return result;
}
static void failed_pnp(void) {
 static WCHAR ids[2048];ULONG count=0;CONFIGRET cr;
 cr=CM_Get_Device_ID_List_SizeW(&count,L"MT8167",CM_GETIDLIST_FILTER_ENUMERATOR|CM_GETIDLIST_FILTER_PRESENT);
 if(cr==CR_SUCCESS && count>1 && count<=2048) {
  cr=CM_Get_Device_ID_ListW(L"MT8167",ids,2048,CM_GETIDLIST_FILTER_ENUMERATOR|CM_GETIDLIST_FILTER_PRESENT);
  if(cr==CR_SUCCESS){WCHAR *p=ids;while(*p){if(prefix(p,L"MT8167\\AFE\\")) {
   DEVINST dev;ULONG status=0,problem=0;cr=CM_Locate_DevNodeW(&dev,p,CM_LOCATE_DEVNODE_NORMAL);
   if(cr==CR_SUCCESS)cr=CM_Get_DevNode_Status(&status,&problem,dev,0);line("pnp-no-filter",cr,status,problem);
  }while(*p)p++;p++;}}
 }
 line("pnp-list",cr,count,0);
}
/* KsCreatePin can return an asynchronous handle. Keep request buffers and
 * OVERLAPPED alive through actual completion; no stack-backed pending I/O. */
static BOOL control(HANDLE handle,DWORD code,void *in,DWORD inbytes,void *out,DWORD outbytes,DWORD *got) {
 OVERLAPPED operation;BOOL ok;DWORD error=ERROR_SUCCESS;
 ZeroMemory(&operation,sizeof(operation));operation.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
 if(!operation.hEvent)return FALSE;
 ok=DeviceIoControl(handle,code,in,inbytes,out,outbytes,got,&operation);
 if(!ok) {
  error=GetLastError();
  if(error==ERROR_IO_PENDING){ok=GetOverlappedResult(handle,&operation,got,TRUE);error=ok?ERROR_SUCCESS:GetLastError();}
 }
 CloseHandle(operation.hEvent);SetLastError(error);return ok;
}
static DWORD snapshot(HANDLE filter,struct a04_audio_snapshot *s) {
 DWORD got=0;ZeroMemory(s,sizeof(*s));
 if(!control(filter,A04_AUDIO_READ_STATUS,NULL,0,s,sizeof(*s),&got))return GetLastError();
 if(got!=sizeof(*s)||s->size!=sizeof(*s)||s->version!=A04_AUDIO_DIAG_VERSION)return ERROR_INVALID_DATA;
 return 0;
}
static void dump(const struct a04_audio_snapshot *s,DWORD phase,DWORD captured_tick) {
 const struct a04_codec_snapshot *c=&s->codec;
 line("snapshot",phase,s->sequence,captured_tick);line("state",s->stage,s->last_status,s->failure_stage);line("transport",s->transport,s->irq_active,s->dma_exposed);
 line("valid",s->valid,c->valid,s->online);line("dma",s->dma_base,s->dma_end,s->dma_bytes);
 line("cur",s->dl1_cur,s->position_bytes,s->dma_exposed);line("irq",s->irq_events,s->irq_faults,s->irq_status);
 line("afe",s->dac0,s->afe_top,s->irq_active);line("lease",c->online,c->leased,c->power_pinned);
 line("flags",s->shared_quarantined,s->digital_retained,c->poisoned);
 line("codec",c->digital_prepared,c->voice_active,c->trim_captured);
 line("analog0",c->analog[0],c->analog[1],c->analog[2]);line("analog1",c->analog[3],c->analog[4],c->top_clock);
 line("speaker",c->spk0,c->spk9,c->spk11);line("trim",c->trim_raw,c->trim_code,c->overcurrent);
 line("rails",c->vaud22,c->vaud28,c->pmic_clock);
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
static int codec_running(const struct a04_audio_snapshot *s) {
 const struct a04_codec_snapshot *c=&s->codec;ULONG mask=A04_CODEC_VALID_PARENT|A04_CODEC_VALID_ANALOG|A04_CODEC_VALID_SPEAKER|A04_CODEC_VALID_RAILS|A04_CODEC_VALID_TRIM_OC;
 return (c->valid&mask)==mask && !c->poisoned && c->trim_captured && c->voice_active &&
   (c->analog[1]&(1u<<15)) && (c->analog[2]&(1u<<8)) && (c->spk0&1u) &&
   (c->spk9&0xf00)==0x100 && !(c->overcurrent&(1u<<14)) &&
   (c->vaud22&0xc000)==0xc000 && (c->vaud28&0xc000)==0xc000;
}
static DWORD run(void) {
 HANDLE filter=INVALID_HANDLE_VALUE,pin=INVALID_HANDLE_VALUE,mutex;HMODULE ks=NULL;CREATE_KS_PIN create;
 struct {KSPIN_CONNECT connect;KSDATAFORMAT_WAVEFORMATEX format;} request;
 typedef char contiguous_format[(offsetof(struct {KSPIN_CONNECT c;KSDATAFORMAT_WAVEFORMATEX f;},f)==sizeof(KSPIN_CONNECT))?1:-1];
 KSRTAUDIO_BUFFER_PROPERTY buffer;KSRTAUDIO_BUFFER mapping;
 struct a04_audio_snapshot s;DWORD error=0,result=1,got=0,i,valid=0,changes=0,previous=0,have_previous=0,codec_ok=0;
 static struct a04_audio_snapshot samples[8],before_stop,after_stop;
 DWORD sample_ticks[8],sample_count=0,sample_error=0,run_error=0,run_tick=0;
 DWORD before_error=0,after_error=0,before_tick=0,after_tick=0,pause_error=0,stop_error=0,pause_tick=0,stop_tick=0;
 int ran=0;(void)sizeof(contiguous_format);
 SetLastError(ERROR_SUCCESS);mutex=CreateMutexW(NULL,FALSE,L"A04BR3AudioFirstPlayback");
 if(!mutex || GetLastError()==ERROR_ALREADY_EXISTS){line("singleton-error",GetLastError(),0,0);if(mutex)CloseHandle(mutex);return 2;}
 if(!open_uart()){uart_lost=1;line("uart-open-error",GetLastError(),0,0);result=3;goto done;}
 line("begin",A04_AUDIO_DIAG_VERSION,GetTickCount(),0);
 ks=LoadLibraryW(L"ksuser.dll");if(!ks){line("ksuser-error",GetLastError(),0,0);result=4;goto done;}
 create=(CREATE_KS_PIN)GetProcAddress(ks,"KsCreatePin");if(!create){line("kscreate-error",GetLastError(),0,0);result=4;goto done;}
 filter=find_filter();if(filter==INVALID_HANDLE_VALUE){failed_pnp();result=5;goto done;}
 error=snapshot(filter,&s);if(error){line("snapshot-error",error,0,0);result=6;goto done;}dump(&s,0,GetTickCount());
 if((s.valid&A04_AUDIO_VALID_PARENT)==0 || !s.codec.online || !s.codec.leased || !s.codec.power_pinned ||
    s.stage!=A04_STAGE_CREATED || s.digital_retained || s.shared_quarantined || s.codec.poisoned){line("not-first",s.stage,s.digital_retained,s.shared_quarantined);result=7;goto done;}
 ZeroMemory(&request,sizeof(request));
 error=identifiers(filter,KSPROPERTY_PIN_INTERFACES,(KSIDENTIFIER *)&request.connect.Interface);
 if(!error)error=identifiers(filter,KSPROPERTY_PIN_MEDIUMS,(KSIDENTIFIER *)&request.connect.Medium);
 if(error){line("pin-identifiers-error",error,0,0);result=8;goto done;}
 request.connect.PinId=0;request.connect.Priority.PriorityClass=KSPRIORITY_NORMAL;
 request.connect.Priority.PrioritySubClass=1;
 request.format.DataFormat.FormatSize=sizeof(request.format);request.format.DataFormat.SampleSize=4;
 request.format.DataFormat.MajorFormat=KSDATAFORMAT_TYPE_AUDIO;request.format.DataFormat.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;
 request.format.DataFormat.Specifier=KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
 request.format.WaveFormatEx.wFormatTag=WAVE_FORMAT_PCM;request.format.WaveFormatEx.nChannels=2;
 request.format.WaveFormatEx.nSamplesPerSec=48000;request.format.WaveFormatEx.wBitsPerSample=16;
 request.format.WaveFormatEx.nBlockAlign=4;request.format.WaveFormatEx.nAvgBytesPerSec=192000;
 error=create(filter,&request.connect,GENERIC_WRITE,&pin);
 line("create-pin",error,request.connect.Interface.Id,request.connect.Medium.Id);
 if(error){pin=INVALID_HANDLE_VALUE;result=9;goto done;}
 error=state(pin,KSSTATE_ACQUIRE);line("acquire",error,0,0);if(error){result=10;goto done;}
 ZeroMemory(&buffer,sizeof(buffer));ZeroMemory(&mapping,sizeof(mapping));
 buffer.Property.Set=KSPROPSETID_RtAudio;buffer.Property.Id=KSPROPERTY_RTAUDIO_BUFFER;
 buffer.Property.Flags=KSPROPERTY_TYPE_GET;buffer.RequestedBufferSize=8192;
 if(!control(pin,IOCTL_KS_PROPERTY,&buffer,sizeof(buffer),&mapping,sizeof(mapping),&got))error=GetLastError();
 else if(got!=sizeof(mapping)||!mapping.BufferAddress||mapping.ActualBufferSize<8192)error=ERROR_INVALID_DATA;
 line("buffer",error,error?0:mapping.ActualBufferSize,error?0:mapping.CallMemoryBarrier);
 if(error){result=11;goto done;}
 error=state(pin,KSSTATE_PAUSE);line("pause-before",error,0,0);if(error){result=12;goto done;}
 if(uart_lost){result=3;goto done;}
 /* Exactly one RUN request. Keep UART output out of the active interval:
  * shared UART queue contention must not delay mute/STOP. */
 run_tick=GetTickCount();run_error=state(pin,KSSTATE_RUN);ran=1;
 if(run_error){result=13;goto observe_failure;}
 result=14;
 for(i=0;i<8;i++) {
  Sleep(37);sample_error=snapshot(filter,&samples[i]);sample_ticks[i]=GetTickCount();
  if(sample_error)break;
  sample_count++;s=samples[i];
  if((s.valid&(A04_AUDIO_VALID_POSITION|A04_AUDIO_VALID_DMA|A04_AUDIO_VALID_AFE))==
      (A04_AUDIO_VALID_POSITION|A04_AUDIO_VALID_DMA|A04_AUDIO_VALID_AFE) && !s.irq_faults && (s.dac0&3)==3 &&
      s.dl1_cur>=s.dma_base && s.dl1_cur<=s.dma_end && s.position_bytes<s.dma_bytes) {
   valid++;if(have_previous && previous!=s.position_bytes)changes++;previous=s.position_bytes;have_previous=1;
  }
  if(codec_running(&s))codec_ok++;
 }
 if(valid>=2 && changes && codec_ok==8)result=0;
observe_failure:
 before_error=snapshot(filter,&before_stop);before_tick=GetTickCount();
 if(ran){pause_error=state(pin,KSSTATE_PAUSE);pause_tick=GetTickCount();if(pause_error && !result)result=15;}
 stop_error=state(pin,KSSTATE_STOP);stop_tick=GetTickCount();if(stop_error && !result)result=15;
 after_error=snapshot(filter,&after_stop);after_tick=GetTickCount();
 if(!after_error) {
  if(((after_stop.valid&A04_AUDIO_VALID_AFE)==0 || (after_stop.codec.valid&A04_CODEC_VALID_SPEAKER)==0 ||
     (after_stop.dac0&2) || (after_stop.codec.spk9&0xf00)) && !result)result=16;
 }else if(!result)result=16;
 /* Everything below is a report of already captured data. The driver keeps
  * its resources pinned; transmission time is not a sampling timestamp. */
 line("run",run_error,run_tick,0);
 if(sample_count)dump(&samples[0],1,sample_ticks[0]);
 for(i=0;i<sample_count;i++) {
  const struct a04_audio_snapshot *sample=&samples[i];
  line("sample",i,sample_ticks[i],sample->valid);
  line("sample-cur",sample->dl1_cur,sample->position_bytes,sample->irq_events);
  line("sample-codec",sample->codec.spk9,sample->codec.overcurrent,sample->irq_faults);
  line("sample-valid",sample->codec.valid,sample->codec.poisoned,sample->failure_stage);
 }
 if(sample_error)line("sample-error",sample_error,sample_count,0);
 if(!before_error)dump(&before_stop,2,before_tick);else line("snapshot-error",before_error,2,0);
 if(ran)line("pause-after",pause_error,pause_tick,0);
 line("stop",stop_error,stop_tick,0);
 if(!after_error)dump(&after_stop,3,after_tick);else line("snapshot-error",after_error,3,0);
 line("observed",valid,changes,codec_ok);
done:
 /* Close is not DMA drain. The kernel deliberately retains exposed resources. */
 if(pin!=INVALID_HANDLE_VALUE)CloseHandle(pin);
 if(filter!=INVALID_HANDLE_VALUE)CloseHandle(filter);
 if(ks)FreeLibrary(ks);
 if(uart_lost && !result)result=3;
 if(log_lost && !result)result=17;
 line("end",result,(DWORD)uart_lost,0); /* c=0: audible success unverified */
 if(uart_lost && !result){result=3;line("uart-queue-error",result,0,0);}
 if(logfile!=INVALID_HANDLE_VALUE)FlushFileBuffers(logfile);
 Sleep(500);CloseHandle(mutex);return result;
}
void mainCRTStartup(void) {
 DWORD result;runid=GetTickCount();open_log();result=run();
 if(uart!=INVALID_HANDLE_VALUE)CloseHandle(uart);
 if(logfile!=INVALID_HANDLE_VALUE)CloseHandle(logfile);
 ExitProcess(result);
}
