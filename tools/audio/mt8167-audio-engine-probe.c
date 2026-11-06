/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Normal Windows AudioSrv/MMDevice/WASAPI probe. No KS pin or tone service.
 * Only fixed PCM, fixed service names and two fixed read/write diagnostics:
 * read-only audio snapshot, existing UART text queue. No command broker. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <winsvc.h>
#include <winioctl.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <devicetopology.h>
#include <propsys.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <stddef.h>
#include "../../drivers/audio/mt8167-afe/diagnostic.h"
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
#ifndef A04_ENGINE_PROBE_SOURCE
#define A04_ENGINE_PROBE_SOURCE 0u
#endif
/* SDK UUIDs/PROPERTYKEY tuples, local names: C MIDL headers only declare IIDs.
 * No dependency on uuid.lib, ksuser.dll, a CRT or an executable payload. */
static const GUID G_Enumerator={0xbcde0395,0xe52f,0x467c,{0x8e,0x3d,0xc4,0x57,0x92,0x91,0x69,0x2e}};
static const GUID I_Enumerator={0xa95664d2,0x9614,0x4f35,{0xa7,0x46,0xde,0x8d,0xb6,0x36,0x17,0xe6}};
static const GUID I_Client={0x1cb9ad4c,0xdbfa,0x4c32,{0xb1,0x78,0xc2,0xf5,0x68,0xa7,0x03,0xb2}};
static const GUID I_Render={0xf294acfc,0x3146,0x4483,{0xa7,0xbf,0xad,0xdc,0xa7,0xc2,0x60,0xe2}};
static const GUID I_Clock={0xcd63314f,0x3fba,0x4a1b,{0x81,0x2c,0xef,0x96,0x35,0x87,0x28,0xe7}};
static const GUID I_Topology={0x2a07407e,0x6497,0x4a18,{0x97,0x87,0x32,0xf7,0x9b,0xd0,0xd9,0x8f}};
static const GUID G_Render={0x65e8773e,0x8f56,0x11d0,{0xa3,0xb9,0,0xa0,0xc9,0x22,0x31,0x96}};
static const GUID G_Pcm={1,0,0x0010,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
static const GUID G_Float={0x00000003,0x0000,0x0010,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
static const PROPERTYKEY K_Instance={{0x78c34fc8,0x104a,0x4aca,{0x9e,0xa4,0x52,0x4d,0x52,0x99,0x6e,0x57}},256};
static const PROPERTYKEY K_Name={{0xa45c254e,0xdf1c,0x4efd,{0x80,0x20,0x67,0xd1,0x46,0xa8,0x50,0xe0}},14};
void *memcpy(void *d,const void *s,size_t n){volatile BYTE *a=d;const volatile BYTE *b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int v,size_t n){volatile BYTE *a=d;while(n--)*a++=(BYTE)v;return d;}
#define RELEASE(type,p) do{if(p){type##_Release(p);(p)=NULL;}}while(0)
#define SAMPLE_MAX 512u
#define TOTAL_FRAMES 144000u
static HANDLE logfile=INVALID_HANDLE_VALUE,uart=INVALID_HANDLE_VALUE,filter=INVALID_HANDLE_VALUE;
static HANDLE stop_event,ready_event;
static DWORD runid,log_error,uart_error,diag_error,sample_count;
static volatile LONG phase;
static WCHAR instance[512];
struct diag_record {DWORD tick,phase_begin,phase_end,error;struct a04_audio_snapshot s;};
static struct diag_record records[SAMPLE_MAX];
struct clock_record {DWORD pass,tick,padding;HRESULT hr;UINT64 position,qpc;};
static struct clock_record clocks[SAMPLE_MAX];
static DWORD clock_count;
static char *text(char *p,const char *s){while(*s)*p++=*s++;return p;}
static char *hex(char *p,DWORD n){static const char d[]="0123456789ABCDEF";unsigned i;for(i=0;i<8;i++)*p++=d[(n>>(28-4*i))&15];return p;}
static int same(const void *a,const void *b,size_t n){const BYTE *x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1;}
static WCHAR upper(WCHAR c){return c>=L'a'&&c<=L'z'?c-32:c;}
static int prefix(const WCHAR *s,const WCHAR *p){while(*p)if(upper(*s++)!=upper(*p++))return 0;return 1;}
static int equal(const WCHAR *a,const WCHAR *b){while(*a&&upper(*a)==upper(*b)){a++;b++;}return !*a&&!*b;}
static int copy_wide(WCHAR *to,DWORD cap,const WCHAR *from){DWORD i;for(i=0;i<cap;i++){to[i]=from[i];if(!from[i])return 1;}to[cap-1]=0;return 0;}
static void line(const char *tag,DWORD a,DWORD b,DWORD c){
 char row[96],*p=row;DWORD written=0,start;
 p=text(p,"A04AUDENG r=");p=hex(p,runid);*p++=' ';p=text(p,tag);
 p=text(p," a=");p=hex(p,a);p=text(p," b=");p=hex(p,b);p=text(p," c=");p=hex(p,c);*p++='\n';
 WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),row,(DWORD)(p-row),&written,NULL);
 if(logfile!=INVALID_HANDLE_VALUE){
  BOOL ok=WriteFile(logfile,row,(DWORD)(p-row),&written,NULL);
  if(!ok&&!log_error){DWORD e=GetLastError();log_error=e?e:ERROR_WRITE_FAULT;}
  else if(ok&&written!=(DWORD)(p-row)&&!log_error)log_error=ERROR_WRITE_FAULT;
 }
 if(uart_error||uart==INVALID_HANDLE_VALUE)return;
 start=GetTickCount();
 while(!DeviceIoControl(uart,A04_UART_SCREEN_SEND,row,(DWORD)(p-row),NULL,0,&written,NULL)){
  DWORD e=GetLastError();if(e!=ERROR_BUSY||GetTickCount()-start>=2000){uart_error=e;break;}Sleep(10);
 }
}
/* Exact UTF-16 identity, four code units per row, no locale/CRT conversion. */
static void wide(const char *tag,const WCHAR *s){DWORD i=0,j,words[2];while(s[i]&&i<1024){words[0]=words[1]=0;for(j=0;j<4&&s[i+j];j++)words[j/2]|=(DWORD)s[i+j]<<(16*(j&1));line(tag,i,words[0],words[1]);i+=j;}line(i==1024&&s[i]?"text-trunc":"text-end",i,0,0);}
static void open_outputs(void){
 WCHAR path[MAX_PATH],*p;DWORD n=GetModuleFileNameW(NULL,path,MAX_PATH),i;char digits[8];
 runid=GetTickCount()^GetCurrentProcessId();
 if(n&&n<MAX_PATH){while(n&&path[n-1]!=L'\\'&&path[n-1]!=L'/')n--;if(n+40<MAX_PATH){
  const WCHAR *name=L"audio-engine-probe-";p=path+n;while(*name)*p++=*name++;hex(digits,runid);for(i=0;i<8;i++)*p++=digits[i];
  name=L".log";while(*name)*p++=*name++;*p=0;logfile=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 }}
 if(logfile==INVALID_HANDLE_VALUE)log_error=GetLastError()?GetLastError():ERROR_INVALID_NAME;
 uart=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
 if(uart==INVALID_HANDLE_VALUE)uart_error=GetLastError();
}
static DWORD ensure_service(SC_HANDLE scm,const WCHAR *name,DWORD index){
 SC_HANDLE service=OpenServiceW(scm,name,SERVICE_QUERY_STATUS);SERVICE_STATUS_PROCESS s;DWORD bytes,start=GetTickCount(),error=0;BOOL requested=FALSE;
 if(!service)return GetLastError();
 for(;;){
  if(!QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,(BYTE *)&s,sizeof(s),&bytes)){error=GetLastError();break;}
  line("service",index,s.dwCurrentState,s.dwWin32ExitCode);
  if(s.dwCurrentState==SERVICE_RUNNING)break;
  if(s.dwCurrentState==SERVICE_STOPPED){if(requested){error=s.dwWin32ExitCode?s.dwWin32ExitCode:ERROR_SERVICE_NOT_ACTIVE;break;}
   {SC_HANDLE starter=OpenServiceW(scm,name,SERVICE_START);if(!starter){error=GetLastError();break;}
    if(!StartServiceW(starter,0,NULL)&&GetLastError()!=ERROR_SERVICE_ALREADY_RUNNING)error=GetLastError();CloseServiceHandle(starter);if(error)break;}requested=TRUE;
  }else if(s.dwCurrentState!=SERVICE_START_PENDING){error=ERROR_SERVICE_CANNOT_ACCEPT_CTRL;break;}
  if(GetTickCount()-start>=15000){error=ERROR_TIMEOUT;break;}Sleep(250);
 }
 CloseServiceHandle(service);return error;
}
static HRESULT property(IMMDevice *device,const PROPERTYKEY *key,PROPVARIANT *value){IPropertyStore *store=NULL;HRESULT hr;ZeroMemory(value,sizeof(*value));hr=IMMDevice_OpenPropertyStore(device,STGM_READ,&store);if(SUCCEEDED(hr)){hr=IPropertyStore_GetValue(store,key,value);IPropertyStore_Release(store);}return hr;}
static HRESULT endpoint_instance(IMMDeviceEnumerator *enumerator,IMMDevice *endpoint,WCHAR *out,DWORD cap){
 IDeviceTopology *topology=NULL;IConnector *connector=NULL;IMMDevice *adapter=NULL;LPWSTR id=NULL;PROPVARIANT value;HRESULT hr;
 ZeroMemory(&value,sizeof(value));
 hr=IMMDevice_Activate(endpoint,&I_Topology,CLSCTX_INPROC_SERVER,NULL,(void **)&topology);if(FAILED(hr))goto done;
 hr=IDeviceTopology_GetConnector(topology,0,&connector);if(FAILED(hr))goto done;
 hr=IConnector_GetDeviceIdConnectedTo(connector,&id);if(FAILED(hr))goto done;
 hr=IMMDeviceEnumerator_GetDevice(enumerator,id,&adapter);if(FAILED(hr))goto done;
 hr=property(adapter,&K_Instance,&value);if(FAILED(hr))goto done;
 if(value.vt!=VT_LPWSTR||!value.pwszVal||!copy_wide(out,cap,value.pwszVal))hr=HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
 done:PropVariantClear(&value);CoTaskMemFree(id);RELEASE(IMMDevice,adapter);RELEASE(IConnector,connector);RELEASE(IDeviceTopology,topology);return hr;
}
static HRESULT select_endpoint(IMMDeviceEnumerator *enumerator,IMMDevice **selected){
 IMMDeviceCollection *collection=NULL;IMMDevice *candidate=NULL;DWORD state;UINT count=0,i,matches=0;WCHAR id[512];HRESULT hr;
 *selected=NULL;hr=IMMDeviceEnumerator_EnumAudioEndpoints(enumerator,eRender,DEVICE_STATEMASK_ALL,&collection);if(FAILED(hr))return hr;
 hr=IMMDeviceCollection_GetCount(collection,&count);line("endpoints",hr,count,0);if(FAILED(hr)||count>64){if(SUCCEEDED(hr))hr=E_UNEXPECTED;goto done;}
 for(i=0;i<count;i++){
  hr=IMMDeviceCollection_Item(collection,i,&candidate);if(FAILED(hr))goto done;
  state=0;hr=IMMDevice_GetState(candidate,&state);line("endpoint",i,hr,state);if(FAILED(hr))goto done;
  if(state==DEVICE_STATE_ACTIVE){
   hr=endpoint_instance(enumerator,candidate,id,512);line("route",i,hr,0);
   if(SUCCEEDED(hr)&&prefix(id,L"MT8167\\AFE\\")){
    PROPVARIANT name;LPWSTR endpointId=NULL;matches++;wide("instance16",id);
    if(SUCCEEDED(IMMDevice_GetId(candidate,&endpointId))){wide("endpoint16",endpointId);CoTaskMemFree(endpointId);}
    if(SUCCEEDED(property(candidate,&K_Name,&name))&&name.vt==VT_LPWSTR&&name.pwszVal)wide("friendly16",name.pwszVal);PropVariantClear(&name);
    if(matches==1){*selected=candidate;IMMDevice_AddRef(candidate);copy_wide(instance,512,id);}
   }
  }
  RELEASE(IMMDevice,candidate);
 }
 line("selected",matches,0,0);hr=matches==1?S_OK:HRESULT_FROM_WIN32(matches?ERROR_DUP_NAME:ERROR_NOT_FOUND);
 done:RELEASE(IMMDevice,candidate);RELEASE(IMMDeviceCollection,collection);if(FAILED(hr))RELEASE(IMMDevice,*selected);return hr;
}
static HANDLE open_filter(void){
 HDEVINFO set;SP_DEVICE_INTERFACE_DATA iface;SP_DEVINFO_DATA dev;DWORD index,need,matches=0;HANDLE result=INVALID_HANDLE_VALUE;
 static union {ULONGLONG align;BYTE bytes[4096];} detail;static WCHAR id[512],path[1024];
 set=SetupDiGetClassDevsW(&G_Render,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);if(set==INVALID_HANDLE_VALUE)return result;
 for(index=0;;index++){
  ZeroMemory(&iface,sizeof(iface));iface.cbSize=sizeof(iface);if(!SetupDiEnumDeviceInterfaces(set,NULL,&G_Render,index,&iface))break;
  ZeroMemory(&dev,sizeof(dev));dev.cbSize=sizeof(dev);((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
  if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,(PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes,sizeof(detail),&need,&dev))continue;
  if(CM_Get_Device_IDW(dev.DevInst,id,512,0)!=CR_SUCCESS||!equal(instance,id))continue;
  {ULONG status=0,problem=0;CONFIGRET cr=CM_Get_DevNode_Status(&status,&problem,dev.DevInst,0);line("pnp",cr,status,problem);if(cr!=CR_SUCCESS||!(status&DN_STARTED)||problem)continue;}
  matches++;if(!copy_wide(path,1024,((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->DevicePath))matches=2;
 }
 SetupDiDestroyDeviceInfoList(set);line("diag-filter",matches,0,0);
 if(matches!=1)SetLastError(matches?ERROR_DUP_NAME:ERROR_NOT_FOUND);
 if(matches==1)result=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
 return result;
}
/* Static I/O storage stays alive even if a broken kernel never cancels an IRP. */
static OVERLAPPED diag_io;static struct a04_audio_snapshot diag_buffer;
static DWORD read_snapshot(struct a04_audio_snapshot *out){
 DWORD bytes=0,error=0,wait;BOOL ok;HANDLE event=diag_io.hEvent;ZeroMemory(&diag_io,sizeof(diag_io));diag_io.hEvent=event;ZeroMemory(&diag_buffer,sizeof(diag_buffer));ResetEvent(event);
 ok=DeviceIoControl(filter,A04_AUDIO_READ_STATUS,NULL,0,&diag_buffer,sizeof(diag_buffer),&bytes,&diag_io);
 if(!ok){error=GetLastError();if(error==ERROR_IO_PENDING){HANDLE events[2]={stop_event,diag_io.hEvent};wait=WaitForMultipleObjects(2,events,FALSE,500);
   if(wait!=WAIT_OBJECT_0+1){error=wait==WAIT_OBJECT_0?ERROR_OPERATION_ABORTED:ERROR_TIMEOUT;CancelIoEx(filter,&diag_io);GetOverlappedResult(filter,&diag_io,&bytes,TRUE);return error;}
   if(!GetOverlappedResult(filter,&diag_io,&bytes,FALSE))return GetLastError();error=0;
  }}
 if(error)return error;
 if(bytes!=sizeof(diag_buffer)||diag_buffer.size!=sizeof(diag_buffer)||diag_buffer.version!=A04_AUDIO_DIAG_VERSION)return ERROR_INVALID_DATA;
 *out=diag_buffer;return 0;
}
static DWORD WINAPI collect(LPVOID unused){
 (void)unused;for(;;){struct diag_record *r;if(sample_count==SAMPLE_MAX){diag_error=ERROR_BUFFER_OVERFLOW;SetEvent(ready_event);break;}
  r=&records[sample_count];r->phase_begin=(DWORD)InterlockedCompareExchange(&phase,0,0);r->tick=GetTickCount();r->error=read_snapshot(&r->s);
  r->phase_end=(DWORD)InterlockedCompareExchange(&phase,0,0);sample_count++;
  if(r->error&&!(r->error==ERROR_OPERATION_ABORTED&&WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0)&&!diag_error)diag_error=r->error;SetEvent(ready_event);
  if(r->error||WaitForSingleObject(stop_event,37)!=WAIT_TIMEOUT)break;
 }return 0;
}
static int sample_valid(const struct a04_audio_snapshot *s){
 DWORD required=A04_AUDIO_VALID_PARENT|A04_AUDIO_VALID_AFE|A04_AUDIO_VALID_POSITION|A04_AUDIO_VALID_DMA;
 return (s->valid&required)==required&&s->online&&s->dma_exposed&&s->dma_bytes&&s->dma_bytes<=1048576&&s->dma_end>=s->dma_base&&
  s->dma_end-s->dma_base==s->dma_bytes-1&&s->dl1_cur>=s->dma_base&&s->dl1_cur<=s->dma_end&&s->position_bytes==s->dl1_cur-s->dma_base&&!(s->position_bytes&3);
}
static DWORD dump_diagnostics(void){DWORD i,valid[2]={0,0},changes[2]={0,0},last[2]={0,0},bad[2]={0,0},observed=0;
 for(i=0;i<sample_count;i++){struct diag_record *r=&records[i];struct a04_audio_snapshot *s=&r->s;DWORD p=r->phase_begin;
  line("sample",i,r->tick,p);line("sample-end",i,r->phase_end,r->error);if(r->error)continue;
  line("state",s->stage,s->last_status,s->failure_stage);line("valid",s->valid,s->codec.valid,s->online);
  line("dma",s->dma_base,s->dma_end,s->dma_bytes);line("cur",s->dl1_cur,s->position_bytes,s->dma_exposed);
  line("irq",s->irq_events,s->irq_faults,s->irq_status);line("afe",s->dac0,s->afe_top,s->irq_active);
  line("codec",s->codec.digital_prepared,s->codec.voice_active,s->codec.poisoned);
  line("speaker",s->codec.spk0,s->codec.spk9,s->codec.overcurrent);line("trim",s->codec.trim_captured,s->codec.trim_code,s->codec.trim_raw);
  line("rails",s->codec.vaud22,s->codec.vaud28,s->codec.power_pinned);line("lease",s->codec.online,s->codec.leased,s->shared_quarantined);
  if(p>=1&&p<=2&&r->phase_end==p){DWORD n=p-1;
   if(s->irq_faults||s->codec.poisoned||((s->codec.valid&A04_CODEC_VALID_TRIM_OC)&&(s->codec.overcurrent&(1u<<14))))bad[n]=1;
   if(sample_valid(s)){if(valid[n]&&last[n]!=s->position_bytes)changes[n]++;last[n]=s->position_bytes;valid[n]++;}
  }
 }
 for(i=0;i<2;i++){line("dma-progress",i,valid[i],changes[i]);line("dma-fault",i,bad[i],0);if(valid[i]>=2&&changes[i]&&!bad[i])observed|=1u<<i;}
 return observed;
}
static int format_kind(const WAVEFORMATEX *w){
 int kind=0;if(w->nSamplesPerSec!=48000||w->nChannels!=2)return 0;
 if(w->wFormatTag==WAVE_FORMAT_PCM&&w->wBitsPerSample==16&&!w->cbSize)kind=16;
 if(w->wFormatTag==WAVE_FORMAT_IEEE_FLOAT&&w->wBitsPerSample==32&&!w->cbSize)kind=32;
 if(w->wFormatTag==WAVE_FORMAT_EXTENSIBLE&&w->cbSize==22){const WAVEFORMATEXTENSIBLE *e=(const WAVEFORMATEXTENSIBLE *)w;
  if(e->dwChannelMask!=3)return 0;
  if(w->wBitsPerSample==16&&e->Samples.wValidBitsPerSample==16&&same(&e->SubFormat,&G_Pcm,sizeof(GUID)))kind=16;
  if(w->wBitsPerSample==32&&e->Samples.wValidBitsPerSample==32&&same(&e->SubFormat,&G_Float,sizeof(GUID)))kind=32;
 }
 return kind&&w->nBlockAlign==(kind==16?4:8)&&w->nAvgBytesPerSec==48000u*w->nBlockAlign?kind:0;
}
/* Embedded program: 1 s 1500 Hz, 1 s silence, 1 s 750 Hz. Peak 1/256 FS.
 * IEEE_FLOAT words encode +/-1/256 exactly; no FP/CRT/ARM conversion helper. */
static void fill(BYTE *p,UINT32 frames,DWORD first,int kind,DWORD *hash){UINT32 i,j,ch;DWORD value,frame;
 for(i=0;i<frames;i++){frame=first+i;value=0;
  if(frame<48000||frame>=96000){DWORD mask=frame<48000?31:63;BOOL positive=(frame&mask)<((mask+1)/2);value=kind==16?(positive?0x80u:0xff80u):(positive?0x3b800000u:0xbb800000u);}
  for(ch=0;ch<2;ch++)for(j=0;j<(kind==16?2u:4u);j++){BYTE b=(BYTE)(value>>(8*j));*p++=b;*hash=(*hash^b)*16777619u;}
 }
}
static HRESULT render_pass(IMMDevice *endpoint,DWORD pass){
 IAudioClient *client=NULL;IAudioRenderClient *render=NULL;IAudioClock *clock=NULL;WAVEFORMATEX *format=NULL;BYTE *data;
 REFERENCE_TIME period=0,minimum=0,latency=0;UINT64 frequency=0,firstClock=0,lastClock=0,qpc=0;UINT32 capacity=0,padding=0,available;
 DWORD submitted=0,hash=2166136261u,start=0,lastSample=0,waitMs=5,step=1;HRESULT hr=S_OK,stopHr=S_OK;int kind=0;BOOL running=FALSE,haveClock=FALSE,drained=FALSE;
 hr=IMMDevice_Activate(endpoint,&I_Client,CLSCTX_INPROC_SERVER,NULL,(void **)&client);line("activate",pass,hr,0);if(FAILED(hr))goto done;
 step=2;hr=IAudioClient_GetMixFormat(client,&format);if(FAILED(hr))goto done;kind=format_kind(format);
 line("mix",pass,format->nSamplesPerSec,format->wFormatTag);line("mix-pcm",format->nChannels,format->wBitsPerSample,format->nBlockAlign);
 if(!kind){hr=AUDCLNT_E_UNSUPPORTED_FORMAT;goto done;}
 step=3;hr=IAudioClient_GetDevicePeriod(client,&period,&minimum);if(FAILED(hr))goto done;
 line("period",pass,(DWORD)period,(DWORD)minimum);if(period>0&&period<=2000000)waitMs=(DWORD)period/20000;if(!waitMs)waitMs=1;if(waitMs>10)waitMs=10;
 step=4;hr=IAudioClient_Initialize(client,AUDCLNT_SHAREMODE_SHARED,0,1000000,0,format,NULL);line("initialize",pass,hr,AUDCLNT_SHAREMODE_SHARED);if(FAILED(hr))goto done;
 step=5;hr=IAudioClient_GetBufferSize(client,&capacity);if(FAILED(hr))goto done;if(!capacity||capacity>48000){hr=E_UNEXPECTED;goto done;}
 step=6;hr=IAudioClient_GetStreamLatency(client,&latency);line("latency",pass,hr,(DWORD)latency);if(FAILED(hr))goto done;
 step=7;hr=IAudioClient_GetService(client,&I_Render,(void **)&render);if(FAILED(hr))goto done;
 step=8;hr=IAudioClient_GetService(client,&I_Clock,(void **)&clock);if(FAILED(hr))goto done;
 step=9;hr=IAudioClock_GetFrequency(clock,&frequency);line("clock-hz",pass,(DWORD)frequency,(DWORD)(frequency>>32));if(FAILED(hr)||!frequency){if(SUCCEEDED(hr))hr=E_UNEXPECTED;goto done;}
 step=10;hr=IAudioRenderClient_GetBuffer(render,capacity,&data);if(FAILED(hr))goto done;fill(data,capacity,0,kind,&hash);
 step=11;hr=IAudioRenderClient_ReleaseBuffer(render,capacity,0);if(FAILED(hr))goto done;submitted=capacity;
 /* Disk evidence is mandatory before PCM; UART is an optional transport. */
 if(!log_error&&!FlushFileBuffers(logfile))log_error=GetLastError()?GetLastError():ERROR_WRITE_FAULT;
 if(log_error){hr=HRESULT_FROM_WIN32(log_error);goto done;}
 step=12;InterlockedExchange(&phase,(LONG)pass+1);hr=IAudioClient_Start(client);if(FAILED(hr)){InterlockedExchange(&phase,0);goto done;}
 running=TRUE;start=lastSample=GetTickCount();
 while(GetTickCount()-start<10000){
  Sleep(waitMs);step=13;hr=IAudioClient_GetCurrentPadding(client,&padding);if(FAILED(hr))break;if(padding>capacity){hr=E_UNEXPECTED;break;}
  if(GetTickCount()-lastSample>=37){UINT64 pos=0;HRESULT clockHr;step=14;clockHr=IAudioClock_GetPosition(clock,&pos,&qpc);lastSample=GetTickCount();
   if(clock_count==SAMPLE_MAX){hr=HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);break;}
   {struct clock_record *r=&clocks[clock_count++];r->pass=pass;r->tick=lastSample;r->padding=padding;r->hr=clockHr;r->position=pos;r->qpc=qpc;}
   if(clockHr!=S_OK){hr=FAILED(clockHr)?clockHr:E_UNEXPECTED;break;}
   if(haveClock&&pos<lastClock){hr=E_UNEXPECTED;break;}if(!haveClock){firstClock=pos;haveClock=TRUE;}lastClock=pos;
  }
  if(submitted==TOTAL_FRAMES&&!padding){drained=TRUE;break;}
  available=capacity-padding;if(available&&submitted<TOTAL_FRAMES){if(available>TOTAL_FRAMES-submitted)available=TOTAL_FRAMES-submitted;
   step=15;hr=IAudioRenderClient_GetBuffer(render,available,&data);if(FAILED(hr))break;fill(data,available,submitted,kind,&hash);
   step=16;hr=IAudioRenderClient_ReleaseBuffer(render,available,0);if(FAILED(hr))break;submitted+=available;
  }
 }
 if(SUCCEEDED(hr)&&(!drained||submitted!=TOTAL_FRAMES||padding||!haveClock||lastClock==firstClock)){step=17;hr=HRESULT_FROM_WIN32(ERROR_TIMEOUT);}
 done:
 if(running){stopHr=IAudioClient_Stop(client);if(SUCCEEDED(hr)&&FAILED(stopHr)){step=18;hr=stopHr;}}
 InterlockedExchange(&phase,0);
 /* No UART/file output occurred in the render loop. Stop before emitting. */
 CoTaskMemFree(format);RELEASE(IAudioClock,clock);RELEASE(IAudioRenderClient,render);RELEASE(IAudioClient,client);
 line("wasapi-stop",pass,stopHr,running);line("submitted",pass,submitted,hash);line("pass-result",pass,hr,SUCCEEDED(hr)?19:step);line("clock-delta",pass,(DWORD)(lastClock-firstClock),(DWORD)((lastClock-firstClock)>>32));return hr;
}
void mainCRTStartup(void){
 SC_HANDLE scm=NULL;IMMDeviceEnumerator *enumerator=NULL;IMMDevice *endpoint=NULL;HANDLE worker=NULL,mutex=NULL;
 HRESULT hr=E_FAIL;DWORD result=1,error=0,i,api=0,dma=0,joined=0;BOOL com=FALSE;
 open_outputs();line("begin",1,GetTickCount(),A04_ENGINE_PROBE_SOURCE);line("expected-src",0x0de214c8u,0x0e4dff2bu,1004);
 line("transport",log_error,uart_error,1); /* c=1: UART optional, disk mandatory */
 if(uart_error)line("uart-optional",uart_error,0,0);
 if(!log_error&&!FlushFileBuffers(logfile))log_error=GetLastError()?GetLastError():ERROR_WRITE_FAULT;
 if(log_error){line("disk-required",log_error,0,0);goto done;}
 SetLastError(ERROR_SUCCESS);mutex=CreateMutexW(NULL,FALSE,L"A04BR3AudioEngineProbe");if(!mutex||GetLastError()==ERROR_ALREADY_EXISTS){line("singleton",GetLastError(),0,0);goto done;}
 scm=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);if(!scm){line("scm",GetLastError(),0,0);goto done;}
 error=ensure_service(scm,L"AudioEndpointBuilder",0);if(!error)error=ensure_service(scm,L"AudioSrv",1);if(error){line("service-error",error,0,0);goto done;}
 hr=CoInitializeEx(NULL,COINIT_MULTITHREADED);line("com",hr,0,0);if(FAILED(hr))goto done;com=TRUE;
 hr=CoCreateInstance(&G_Enumerator,NULL,CLSCTX_INPROC_SERVER,&I_Enumerator,(void **)&enumerator);if(FAILED(hr))goto done;
 for(i=0;i<40;i++){hr=select_endpoint(enumerator,&endpoint);if(SUCCEEDED(hr)||hr!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND))break;Sleep(250);}
 if(FAILED(hr))goto done;
 filter=open_filter();if(filter==INVALID_HANDLE_VALUE){line("filter-error",GetLastError(),0,0);goto done;}
 stop_event=CreateEventW(NULL,TRUE,FALSE,NULL);ready_event=CreateEventW(NULL,TRUE,FALSE,NULL);diag_io.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
 if(!stop_event||!ready_event||!diag_io.hEvent){line("event-error",GetLastError(),0,0);goto done;}
 worker=CreateThread(NULL,0,collect,NULL,0,NULL);if(!worker){line("thread-error",GetLastError(),0,0);goto done;}
 if(WaitForSingleObject(ready_event,2000)!=WAIT_OBJECT_0||diag_error){line("diag-ready",diag_error,0,0);goto done;}
 hr=render_pass(endpoint,0);if(FAILED(hr))goto done;api|=1;if(log_error)goto done;
 hr=render_pass(endpoint,1);if(FAILED(hr))goto done;api|=2;
 result=0;
 done:
 if(worker){SetEvent(stop_event);CancelIoEx(filter,&diag_io);joined=WaitForSingleObject(worker,2000)==WAIT_OBJECT_0;
  if(joined){dma=dump_diagnostics();CloseHandle(worker);}else{line("diag-stuck",ERROR_TIMEOUT,0,0);result=4;}
 }
 for(i=0;i<clock_count;i++){struct clock_record *r=&clocks[i];line("clock",r->pass,r->tick,r->hr);line("clock-pos",(DWORD)r->position,(DWORD)(r->position>>32),r->padding);line("clock-qpc",(DWORD)r->qpc,(DWORD)(r->qpc>>32),0);}
 line("api-hr",hr,api,0);line("audible",0,0,0);
 RELEASE(IMMDevice,endpoint);RELEASE(IMMDeviceEnumerator,enumerator);if(com)CoUninitialize();if(scm)CloseServiceHandle(scm);if(mutex)CloseHandle(mutex);
 if(!worker||joined){if(filter!=INVALID_HANDLE_VALUE)CloseHandle(filter);if(stop_event)CloseHandle(stop_event);if(ready_event)CloseHandle(ready_event);if(diag_io.hEvent)CloseHandle(diag_io.hEvent);}
 if(!result&&(api!=3||dma!=3||diag_error))result=3;
 line("summary",api,dma,result);line("output-state",log_error,uart_error,diag_error);if(log_error)result=5;
 line("end",result,GetTickCount(),uart_error);if(log_error)result=5;
 if(logfile!=INVALID_HANDLE_VALUE){if(!FlushFileBuffers(logfile))result=5;CloseHandle(logfile);}if(uart!=INVALID_HANDLE_VALUE)CloseHandle(uart);
 ExitProcess(result);
}
