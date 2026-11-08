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
#include "../../drivers/input/mt8167-accdet/ioctl.h"
#include "mt8167_silent_pcm.h"
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
#define SAMPLE_MAX 7u
#define OUTPUT_CAPACITY 16384u
#define TOTAL_FRAMES 48000u
static int want_hp;
static DWORD pcm_executed;
static HANDLE logfile=INVALID_HANDLE_VALUE,uart=INVALID_HANDLE_VALUE,filter=INVALID_HANDLE_VALUE;
static HANDLE stop_event,ready_event;
static DWORD runid,log_error,uart_error,diag_error,sample_count;
static volatile LONG phase;
static WCHAR instance[512];
struct diag_record {DWORD tick,phase_begin,phase_end,error;struct a04_audio_snapshot s;};
static struct diag_record records[SAMPLE_MAX];
struct clock_record {DWORD pass,tick,padding;HRESULT hr;UINT64 position,qpc;};
static struct clock_record clocks[4];
static DWORD clock_seen[2],clock_valid[4];
struct aggregate {DWORD observed,valid,changes,irq_changes,bad,last_pos,last_irq,first,last;};
static struct aggregate progress[2];
static BYTE pending_output[OUTPUT_CAPACITY];static DWORD pending_bytes,total_output;
static void flush_output(void){DWORD got=0;BOOL ok;
 if(!pending_bytes)return;
 ok=WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pending_output,pending_bytes,&got,NULL);
 if((!ok||got!=pending_bytes)&&!log_error)log_error=ERROR_WRITE_FAULT;
 if(logfile!=INVALID_HANDLE_VALUE){ok=WriteFile(logfile,pending_output,pending_bytes,&got,NULL);
  if((!ok||got!=pending_bytes)&&!log_error)log_error=ERROR_WRITE_FAULT;
 }
 pending_bytes=0;
}
static char *text(char *p,const char *s){while(*s)*p++=*s++;return p;}
static char *hex(char *p,DWORD n){static const char d[]="0123456789ABCDEF";unsigned i;for(i=0;i<8;i++)*p++=d[(n>>(28-4*i))&15];return p;}
static int same(const void *a,const void *b,size_t n){const BYTE *x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1;}
static WCHAR upper(WCHAR c){return c>=L'a'&&c<=L'z'?c-32:c;}
static int prefix(const WCHAR *s,const WCHAR *p){while(*p)if(upper(*s++)!=upper(*p++))return 0;return 1;}
static int equal(const WCHAR *a,const WCHAR *b){while(*a&&upper(*a)==upper(*b)){a++;b++;}return !*a&&!*b;}
static int copy_wide(WCHAR *to,DWORD cap,const WCHAR *from){DWORD i;for(i=0;i<cap;i++){to[i]=from[i];if(!from[i])return 1;}to[cap-1]=0;return 0;}
static void line(const char *tag,DWORD a,DWORD b,DWORD c){
 char row[96],*p=row;DWORD count;
 p=text(p,"A04AUDENG r=");p=hex(p,runid);*p++=' ';p=text(p,tag);
 p=text(p," a=");p=hex(p,a);p=text(p," b=");p=hex(p,b);p=text(p," c=");p=hex(p,c);*p++='\n';
 count=(DWORD)(p-row);
 /* Complete bounded stdout/private-file evidence, not thousands of sync
  * UART IOCTLs and write-through writes. Keep room for final result rows. */
 if(count>OUTPUT_CAPACITY-total_output){if(!log_error)log_error=ERROR_BUFFER_OVERFLOW;return;}
 if(count>OUTPUT_CAPACITY-pending_bytes)flush_output();
 memcpy(pending_output+pending_bytes,row,count);pending_bytes+=count;total_output+=count;
}
/* Exact UTF-16 identity, four code units per row, no locale/CRT conversion. */
static void wide(const char *tag,const WCHAR *s){DWORD i=0,j,words[2];while(s[i]&&i<128){words[0]=words[1]=0;for(j=0;j<4&&s[i+j];j++)words[j/2]|=(DWORD)s[i+j]<<(16*(j&1));line(tag,i,words[0],words[1]);i+=j;}line(i==128&&s[i]?"text-trunc":"text-end",i,0,0);}
static void open_outputs(void){
 WCHAR path[MAX_PATH],*p;DWORD n=GetModuleFileNameW(NULL,path,MAX_PATH),i;char digits[8];
 runid=GetTickCount()^GetCurrentProcessId();
 if(n&&n<MAX_PATH){while(n&&path[n-1]!=L'\\'&&path[n-1]!=L'/')n--;if(n+40<MAX_PATH){
  const WCHAR *name=L"mt8167-silent-pcm-";p=path+n;while(*name)*p++=*name++;hex(digits,runid);for(i=0;i<8;i++)*p++=digits[i];
  name=L".log";while(*name)*p++=*name++;*p=0;logfile=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 }}
 if(logfile==INVALID_HANDLE_VALUE)log_error=GetLastError()?GetLastError():ERROR_INVALID_NAME;
 /* Disk/stdout are authoritative. This bounded client does not submit UART
  * requests; the owning fixed service already reports client start/end. */
 uart_error=ERROR_NOT_SUPPORTED;
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
static HRESULT endpoint_instance(IMMDeviceEnumerator *enumerator,IMMDevice *endpoint,WCHAR *out,DWORD cap,int *route){
 IDeviceTopology *topology=NULL;IConnector *connector=NULL;IMMDevice *adapter=NULL;LPWSTR id=NULL;PROPVARIANT value;HRESULT hr;
 ZeroMemory(&value,sizeof(value));
 hr=IMMDevice_Activate(endpoint,&I_Topology,CLSCTX_INPROC_SERVER,NULL,(void **)&topology);if(FAILED(hr))goto done;
 hr=IDeviceTopology_GetConnector(topology,0,&connector);if(FAILED(hr))goto done;
 hr=IConnector_GetDeviceIdConnectedTo(connector,&id);if(FAILED(hr))goto done;
 *route=mt8167_pcm_reference(id,L"Mt8167HeadphoneTopology")?2:mt8167_pcm_reference(id,L"Mt8167Topology")?1:0;
 hr=IMMDeviceEnumerator_GetDevice(enumerator,id,&adapter);if(FAILED(hr))goto done;
 hr=property(adapter,&K_Instance,&value);if(FAILED(hr))goto done;
 if(value.vt!=VT_LPWSTR||!value.pwszVal||!copy_wide(out,cap,value.pwszVal))hr=HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
 done:PropVariantClear(&value);CoTaskMemFree(id);RELEASE(IMMDevice,adapter);RELEASE(IConnector,connector);RELEASE(IDeviceTopology,topology);return hr;
}
static HRESULT select_endpoint(IMMDeviceEnumerator *enumerator,IMMDevice **selected){
 IMMDeviceCollection *collection=NULL;IMMDevice *candidate=NULL;DWORD state;UINT count=0,i,matches=0;WCHAR id[512];HRESULT hr;int route=0;
 *selected=NULL;hr=IMMDeviceEnumerator_EnumAudioEndpoints(enumerator,eRender,DEVICE_STATEMASK_ALL,&collection);if(FAILED(hr))return hr;
 hr=IMMDeviceCollection_GetCount(collection,&count);line("endpoints",hr,count,0);if(FAILED(hr)||count>64){if(SUCCEEDED(hr))hr=E_UNEXPECTED;goto done;}
 for(i=0;i<count;i++){
  hr=IMMDeviceCollection_Item(collection,i,&candidate);if(FAILED(hr))goto done;
  state=0;hr=IMMDevice_GetState(candidate,&state);line("endpoint",i,hr,state);if(FAILED(hr))goto done;
  if(state==DEVICE_STATE_ACTIVE){
   hr=endpoint_instance(enumerator,candidate,id,512,&route);line("route",i,hr,0);
   if(SUCCEEDED(hr)&&route==(want_hp?2:1)&&prefix(id,L"MT8167\\AFE\\")){
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
/* Enumerate the devnode even when Code 10 prevented any KS interface or
 * MMDevice endpoint. Unavailable ProblemStatus is never reported as success. */
static void report_devnode(void){
 static const DEVPROPKEY problem_key={{0x4340a6c5,0x93fa,0x4706,{0x97,0x2c,0x7b,0x64,0x80,0x08,0xa5,0xa7}},12};
 HDEVINFO set=SetupDiGetClassDevsW(NULL,L"MT8167",NULL,DIGCF_PRESENT|DIGCF_ALLCLASSES);
 SP_DEVINFO_DATA dev;DWORD i,matches=0;WCHAR id[512];
 if(set==INVALID_HANDLE_VALUE){line("devnode-enum",GetLastError(),0,0);return;}
 for(i=0;;i++){
  ZeroMemory(&dev,sizeof(dev));dev.cbSize=sizeof(dev);
  if(!SetupDiEnumDeviceInfo(set,i,&dev)){DWORD e=GetLastError();if(e!=ERROR_NO_MORE_ITEMS)line("devnode-enum",e,i,0);break;}
  if(CM_Get_Device_IDW(dev.DevInst,id,512,0)!=CR_SUCCESS||!prefix(id,L"MT8167\\AFE\\"))continue;
  {ULONG status=0,problem=0;DWORD raw=0,bytes=0,error=0;DEVPROPTYPE type=0;DEVINST parent;
   CONFIGRET cr=CM_Get_DevNode_Status(&status,&problem,dev.DevInst,0);matches++;wide("devnode16",id);line("devnode",cr,status,problem);
   if(!SetupDiGetDevicePropertyW(set,&dev,&problem_key,&type,(BYTE *)&raw,sizeof(raw),&bytes,0))error=GetLastError();
   else if(type!=DEVPROP_TYPE_NTSTATUS||bytes!=sizeof(raw))error=ERROR_INVALID_DATA;
   line("problem-status",error,type,error?0:raw);
   cr=CM_Get_Parent(&parent,dev.DevInst,0);line("parent-query",cr,0,0);
   if(cr==CR_SUCCESS){
    if(CM_Get_Device_IDW(parent,id,512,0)==CR_SUCCESS)wide("parent16",id);
    status=problem=0;cr=CM_Get_DevNode_Status(&status,&problem,parent,0);line("parent-node",cr,status,problem);
   }
  }
 }
 SetupDiDestroyDeviceInfoList(set);line("devnode-count",matches,0,0);
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
  if(!mt8167_pcm_reference(((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->DevicePath,L"Mt8167WaveRT"))continue;
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
static int sample_valid(const struct a04_audio_snapshot *s);
static DWORD WINAPI collect(LPVOID unused){
 DWORD initial=0;(void)unused;
 for(;;){struct diag_record r;DWORD phase_now=(DWORD)InterlockedCompareExchange(&phase,0,0);
  if(initial&&phase_now==0){if(WaitForSingleObject(stop_event,100)!=WAIT_TIMEOUT)break;continue;}
  ZeroMemory(&r,sizeof(r));r.phase_begin=phase_now;r.tick=GetTickCount();r.error=read_snapshot(&r.s);
  r.phase_end=(DWORD)InterlockedCompareExchange(&phase,0,0);
  if(r.error&&!(r.error==ERROR_OPERATION_ABORTED&&WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0)&&!diag_error)diag_error=r.error;
  if(!initial){records[0]=r;sample_count=1;initial=1;SetEvent(ready_event);}
  if(r.error){if(!records[5].tick)records[5]=r;break;}
  if(r.phase_begin>=1&&r.phase_begin<=2&&r.phase_end==r.phase_begin){
   DWORD n=r.phase_begin-1;struct aggregate *g=&progress[n];g->observed++;
   if(r.s.irq_faults||r.s.codec.poisoned||((r.s.codec.valid&A04_CODEC_VALID_TRIM_OC)&&(r.s.codec.overcurrent&(1u<<14))))g->bad++;
   if(!sample_valid(&r.s)&&!records[5].tick)records[5]=r;
   if(sample_valid(&r.s)){
    if(g->valid){if(g->last_pos!=r.s.position_bytes)g->changes++;if(g->last_irq!=r.s.irq_events)g->irq_changes++;}
    else {g->first=r.tick;records[1+n*2]=r;}
    g->last=r.tick;g->last_pos=r.s.position_bytes;g->last_irq=r.s.irq_events;g->valid++;records[2+n*2]=r;
   }
  }
  if(WaitForSingleObject(stop_event,100)!=WAIT_TIMEOUT)break;
 }
 return 0;
}
static int sample_valid(const struct a04_audio_snapshot *s){
 DWORD required=A04_AUDIO_VALID_PARENT|A04_AUDIO_VALID_AFE|A04_AUDIO_VALID_POSITION|A04_AUDIO_VALID_DMA;
 return (s->valid&required)==required&&s->online&&s->dma_exposed&&s->dma_bytes&&s->dma_bytes<=1048576&&s->dma_end>=s->dma_base&&
  s->dma_end-s->dma_base==s->dma_bytes-1&&s->stage==A04_STAGE_RUNNING&&!s->last_status&&!s->failure_stage&&s->irq_active&&(s->dac0&2)&&s->dl1_cur>=s->dma_base&&s->dl1_cur<=s->dma_end&&s->position_bytes==s->dl1_cur-s->dma_base&&!(s->position_bytes&3);
}
static void dump_representatives(void){DWORD i;
 for(i=0;i<SAMPLE_MAX;i++){struct diag_record *r=&records[i];struct a04_audio_snapshot *s=&r->s;DWORD p=r->phase_begin;
  if(!r->tick)continue;
  line("sample",i,r->tick,p);line("sample-end",i,r->phase_end,r->error);if(r->error)continue;
  line("state",s->stage,s->last_status,s->failure_stage);line("valid",s->valid,s->codec.valid,s->online);
  line("dma",s->dma_base,s->dma_end,s->dma_bytes);line("cur",s->dl1_cur,s->position_bytes,s->dma_exposed);
  line("irq",s->irq_events,s->irq_faults,s->irq_status);line("afe",s->dac0,s->afe_top,s->irq_active);
  line("codec",s->codec.digital_prepared,s->codec.voice_active,s->codec.poisoned);
  line("speaker",s->codec.spk0,s->codec.spk9,s->codec.overcurrent);line("trim",s->codec.trim_captured,s->codec.trim_code,s->codec.trim_raw);
  line("rails",s->codec.vaud22,s->codec.vaud28,s->codec.power_pinned);line("lease",s->codec.online,s->codec.leased,s->shared_quarantined);
 }
 }
static DWORD aggregate_diagnostics(void){DWORD i,observed=0;
 for(i=0;i<2;i++){struct aggregate *g=&progress[i];
  line("dma-progress",i,g->valid,g->changes);line("irq-progress",i,g->irq_changes,g->observed);
  line("dma-fault",i,g->bad,0);line("sample-span",i,g->first,g->last);
  if(g->valid>=2&&g->changes&&g->irq_changes&&!g->bad)observed|=1u<<i;
 }
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
/* The only payload writer is shared with the zero-byte verification test. */
static void fill(BYTE*p,UINT32 frames,DWORD first,int kind,DWORD*hash){(void)first;mt8167_pcm_silence(p,frames,kind,hash);}
static DWORD hp_inserted(void){
 HDEVINFO set;SP_DEVICE_INTERFACE_DATA i={0};DWORD n,need,matches=0,error=0,got=0;HANDLE h=INVALID_HANDLE_VALUE;OVERLAPPED op={0};
 static union{ULONGLONG align;BYTE bytes[4096];}detail;static WCHAR path[1024];struct mt8167_accdet_socket sample={0};
 set=SetupDiGetClassDevsW(&GUID_MT8167_ACCDET_SOCKET,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE)return GetLastError();
 for(n=0;n<16;n++){i.cbSize=sizeof(i);if(!SetupDiEnumDeviceInterfaces(set,NULL,&GUID_MT8167_ACCDET_SOCKET,n,&i))break;
  ((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
  if(!SetupDiGetDeviceInterfaceDetailW(set,&i,(PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes,sizeof(detail),&need,NULL)){error=GetLastError();break;}
  if(!copy_wide(path,1024,((PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detail.bytes)->DevicePath)){error=ERROR_BUFFER_OVERFLOW;break;}matches++;
 }
 SetupDiDestroyDeviceInfoList(set);if(error)return error;if(matches!=1||n==16)return matches?ERROR_DUP_NAME:ERROR_NOT_FOUND;
 h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);if(h==INVALID_HANDLE_VALUE)return GetLastError();
 op.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);if(!op.hEvent){error=GetLastError();goto done;}
 if(!DeviceIoControl(h,IOCTL_MT8167_ACCDET_READ_SOCKET,NULL,0,&sample,sizeof(sample),&got,&op)){
  error=GetLastError();if(error==ERROR_IO_PENDING){
   if(WaitForSingleObject(op.hEvent,2000)!=WAIT_OBJECT_0){CancelIoEx(h,&op);if(WaitForSingleObject(op.hEvent,2000)!=WAIT_OBJECT_0)ExitProcess(ERROR_TIMEOUT);GetOverlappedResult(h,&op,&got,FALSE);error=ERROR_TIMEOUT;}
   else error=GetOverlappedResult(h,&op,&got,FALSE)?0:GetLastError();
  }
 }
 if(!error&&(got!=sizeof(sample)||sample.size!=sizeof(sample)||sample.version!=1||sample.qualified!=1||sample.inserted>1))error=ERROR_INVALID_DATA;
 if(!error&&!sample.inserted)error=ERROR_NOT_READY;
 line("hp-real-sense",error,sample.qualified,sample.inserted);
 done:if(op.hEvent)CloseHandle(op.hEvent);CloseHandle(h);return error;
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
 step=6;{HRESULT observed=IAudioClient_GetStreamLatency(client,&latency);line("latency-observation",pass,observed,SUCCEEDED(observed)?(DWORD)latency:0);/* Optional latency evidence never grants or blocks PCM admission. */}
 step=7;hr=IAudioClient_GetService(client,&I_Render,(void **)&render);if(FAILED(hr))goto done;
 step=8;hr=IAudioClient_GetService(client,&I_Clock,(void **)&clock);if(FAILED(hr))goto done;
 step=9;hr=IAudioClock_GetFrequency(clock,&frequency);line("clock-hz",pass,(DWORD)frequency,(DWORD)(frequency>>32));if(FAILED(hr)||!frequency){if(SUCCEEDED(hr))hr=E_UNEXPECTED;goto done;}
 step=10;hr=IAudioRenderClient_GetBuffer(render,capacity,&data);if(FAILED(hr))goto done;fill(data,capacity,0,kind,&hash);
 step=11;hr=IAudioRenderClient_ReleaseBuffer(render,capacity,0);if(FAILED(hr))goto done;submitted=capacity;
 /* Disk evidence is mandatory before PCM; UART is an optional transport. */
 flush_output();if(!log_error&&!FlushFileBuffers(logfile))log_error=GetLastError()?GetLastError():ERROR_WRITE_FAULT;
 if(log_error){hr=HRESULT_FROM_WIN32(log_error);goto done;}
 step=12;InterlockedExchange(&phase,(LONG)pass+1);hr=IAudioClient_Start(client);if(FAILED(hr)){InterlockedExchange(&phase,0);goto done;}
 running=TRUE;pcm_executed=1;start=lastSample=GetTickCount();
 while(GetTickCount()-start<10000){
  Sleep(waitMs);step=13;hr=IAudioClient_GetCurrentPadding(client,&padding);if(FAILED(hr))break;if(padding>capacity){hr=E_UNEXPECTED;break;}
  if(GetTickCount()-lastSample>=37){UINT64 pos=0;HRESULT clockHr;step=14;clockHr=IAudioClock_GetPosition(clock,&pos,&qpc);lastSample=GetTickCount();
   {DWORD k=pass*2+(clock_seen[pass]++?1:0);struct clock_record *r=&clocks[k];clock_valid[k]=1;r->pass=pass;r->tick=lastSample;r->padding=padding;r->hr=clockHr;r->position=pos;r->qpc=qpc;}
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
 open_outputs();line("begin",1,GetTickCount(),A04_ENGINE_PROBE_SOURCE);line("audio-profile",1033,0,0);
 line("transport",log_error,uart_error,1); /* c=1: UART optional, disk mandatory */
 if(uart_error)line("uart-optional",uart_error,0,0);
 flush_output();if(!log_error&&!FlushFileBuffers(logfile))log_error=GetLastError()?GetLastError():ERROR_WRITE_FAULT;
 if(log_error){line("disk-required",log_error,0,0);goto done;}
 SetLastError(ERROR_SUCCESS);mutex=CreateMutexW(NULL,FALSE,L"MT8167SilentPcmConsumer");if(!mutex||GetLastError()==ERROR_ALREADY_EXISTS){line("singleton",GetLastError(),0,0);goto done;}
 {const WCHAR*p=GetCommandLineW();if(*p==L'"'){p++;while(*p&&*p!=L'"')p++;if(*p)p++;}else while(*p&&*p!=L' '&&*p!=L'\t')p++;while(*p==L' '||*p==L'\t')p++;
  if(equal(p,L"--speaker-silence"))want_hp=0;else if(equal(p,L"--headphone-silence"))want_hp=1;else{hr=HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);goto done;}}
 line("fixed-route",want_hp,48000,0);report_devnode();
 if(want_hp&&(error=hp_inserted())!=0){line("HP_NOT_EXECUTED",error,0,0);hr=HRESULT_FROM_WIN32(error);result=ERROR_NOT_READY;goto done;}
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
 /* Exactly one bounded one-second zero PCM lifetime per invocation. */
 result=0;
 done:
 if(worker){SetEvent(stop_event);CancelIoEx(filter,&diag_io);joined=WaitForSingleObject(worker,2000)==WAIT_OBJECT_0;
  if(joined){
   /* One read-only terminal observation, not a stop/drain command. */
   ResetEvent(stop_event);records[6].tick=GetTickCount();records[6].error=read_snapshot(&records[6].s);
   line("stop-snapshot",records[6].error,records[6].s.stage,records[6].s.dac0);
   if(records[6].error&&!diag_error)diag_error=records[6].error;
   dma=aggregate_diagnostics();
   if(!result&&(api!=1||(dma&1)!=1||diag_error))result=3;
   line("api-hr",hr,api,0);line("evidence-summary",api,dma,result);
   /* Authoritative counters first, before optional representative detail.
    * This is not an END marker; normal completion still requires final flush. */
   flush_output();if(logfile!=INVALID_HANDLE_VALUE&&!FlushFileBuffers(logfile)&&!log_error)log_error=ERROR_WRITE_FAULT;
   dump_representatives();CloseHandle(worker);
  }else{line("diag-stuck",ERROR_TIMEOUT,0,0);result=4;}
 }
 for(i=0;i<4;i++){struct clock_record *r=&clocks[i];if(!clock_valid[i])continue;line("clock",r->pass,r->tick,r->hr);line("clock-pos",(DWORD)r->position,(DWORD)(r->position>>32),r->padding);line("clock-qpc",(DWORD)r->qpc,(DWORD)(r->qpc>>32),0);}
 line("api-hr",hr,api,0);if(want_hp&&!pcm_executed)line("HP_NOT_EXECUTED",(DWORD)hr,0,0);line("PCM_EXECUTED",pcm_executed,want_hp,0);line("audible",0,0,0);
 RELEASE(IMMDevice,endpoint);RELEASE(IMMDeviceEnumerator,enumerator);if(com)CoUninitialize();if(scm)CloseServiceHandle(scm);if(mutex)CloseHandle(mutex);
 if(!worker||joined){if(filter!=INVALID_HANDLE_VALUE)CloseHandle(filter);if(stop_event)CloseHandle(stop_event);if(ready_event)CloseHandle(ready_event);if(diag_io.hEvent)CloseHandle(diag_io.hEvent);}
 if(!result&&(api!=1||(dma&1)!=1||diag_error))result=3;
 line("summary",api,dma,result);line("output-state",log_error,uart_error,diag_error);if(log_error)result=5;
 line("end",result,GetTickCount(),uart_error);if(log_error)result=5;
 flush_output();if(logfile!=INVALID_HANDLE_VALUE){if(!FlushFileBuffers(logfile))result=5;CloseHandle(logfile);}if(uart!=INVALID_HANDLE_VALUE)CloseHandle(uart);
 ExitProcess(result);
}
