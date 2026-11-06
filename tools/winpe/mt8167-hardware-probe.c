/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One-shot LocalSystem launcher. Fixed signed targets; no shell or user args.
 * Reuses MT8167DeviceInstall b508de8's SCM/self-disable/signing structure. */
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsvc.h>
#include <winioctl.h>
#include <wintrust.h>
#include <softpub.h>
#include <cfgmgr32.h>
#include <newdev.h>
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"

#ifndef A04_HARDWARE_PROBE_ENGINE_PCM
#define A04_HARDWARE_PROBE_ENGINE_PCM 0
#endif
#if A04_HARDWARE_PROBE_ENGINE_PCM != 0 && A04_HARDWARE_PROBE_ENGINE_PCM != 1
#error Only fixed tone or EnginePcm probe builds are supported
#endif
#if A04_HARDWARE_PROBE_ENGINE_PCM
static const WCHAR service_name[]=L"MT8167EngineProbe";
static const WCHAR service_exe[]=L"MT8167EngineProbe.exe";
static const WCHAR *const clients[2]={L"mt6630-wlan-probe.exe",L"mt8167-audio-engine-probe.exe"};
#else
static const WCHAR service_name[]=L"MT8167HardwareProbe";
static const WCHAR service_exe[]=L"MT8167HardwareProbe.exe";
static const WCHAR *const clients[2]={L"mt6630-wlan-probe.exe",L"mt8167-audio-first-playback.exe"};
#endif
static const WCHAR *const stdout_names[2]={L"wlan.stdout.txt",L"audio.stdout.txt"};
static const WCHAR *const stderr_names[2]={L"wlan.stderr.txt",L"audio.stderr.txt"};
static const DWORD limits[2]={600000u,120000u};
static WCHAR directory[MAX_PATH],log_directory[MAX_PATH],base_directory[MAX_PATH],executable[MAX_PATH],marker[MAX_PATH];
static HANDLE base_handle=INVALID_HANDLE_VALUE,log_directory_handle=INVALID_HANDLE_VALUE;
static HANDLE directory_handle=INVALID_HANDLE_VALUE,logfile=INVALID_HANDLE_VALUE,stop_event;
static SERVICE_STATUS_HANDLE status_handle;
static SERVICE_STATUS service_status;
static DWORD run_id,log_error,uart_error;
static int uart_quarantined,child_uncertain;
enum { EV_BEGIN=1,EV_DISABLED,EV_ATTEMPTED,EV_SIGNATURE,EV_START,EV_EXIT,
       EV_TIMEOUT,EV_CANCEL,EV_ERROR,EV_END,EV_TERMINATE,EV_LOG_ERROR,EV_UART,EV_FINAL,EV_POWER_SET,EV_POWER_CLEAR,EV_POWER_CLOSE,EV_MODE,
       EV_PNP_BEFORE,EV_PNP_AFTER,EV_PNP_LIST_CR,EV_PNP_LOCATE_CR,EV_PNP_PROPERTY_CR,
       EV_PNP_MATCHES,EV_PNP_STATUS_CR,EV_PNP_STATUS,EV_PNP_PROBLEM,EV_PNP_STARTED,
       EV_PNP_PACKAGE,EV_PNP_API_BEGIN,EV_PNP_API_SUCCESS,EV_PNP_WIN32,EV_PNP_REBOOT,EV_PNP_RESULT };

void *memset(void *p,int v,size_t n){volatile BYTE *b=p;while(n--)*b++=(BYTE)v;return p;}
void *memcpy(void *p,const void *q,size_t n){volatile BYTE *b=p;const volatile BYTE *a=q;while(n--)*b++=*a++;return p;}
static char *text(char *p,const char *s){while(*s)*p++=*s++;return p;}
static char *hex(char *p,DWORD n){static const char d[]="0123456789ABCDEF";unsigned i;for(i=0;i<8;i++)*p++=d[(n>>(28-4*i))&15];return p;}
static DWORD path(WCHAR out[MAX_PATH],const WCHAR *base,const WCHAR *name)
{
 unsigned n=0,i=0;while(base[n]){if(n>=MAX_PATH-2)return ERROR_INSUFFICIENT_BUFFER;out[n]=base[n];n++;}
 out[n++]=L'\\';while(name[i]){if(n>=MAX_PATH-1)return ERROR_INSUFFICIENT_BUFFER;out[n++]=name[i++];}out[n]=0;return 0;
}
struct uart_packet {OVERLAPPED operation;char text[96];DWORD bytes;};
static DWORD uart_record(const char *line,DWORD length)
{
 struct uart_packet *packet;HANDLE port;DWORD error=0,start=GetTickCount(),wait;int pending=0;
 if(uart_quarantined)return ERROR_IO_INCOMPLETE;
 packet=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*packet));if(!packet)return ERROR_NOT_ENOUGH_MEMORY;
 memcpy(packet->text,line,length);
 packet->operation.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
 if(!packet->operation.hEvent){error=GetLastError();HeapFree(GetProcessHeap(),0,packet);return error;}
 port=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
 if(port==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 for(;;){
  ResetEvent(packet->operation.hEvent);
  if(DeviceIoControl(port,A04_UART_SCREEN_SEND,packet->text,length,NULL,0,&packet->bytes,&packet->operation)){error=0;break;}
  error=GetLastError();
  if(error==ERROR_IO_PENDING){
   wait=WaitForSingleObject(packet->operation.hEvent,1000);
   if(wait!=WAIT_OBJECT_0){
    error=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();(void)CancelIoEx(port,&packet->operation);
    if(WaitForSingleObject(packet->operation.hEvent,250)!=WAIT_OBJECT_0){pending=1;uart_quarantined=1;}
    break;
   }
   error=GetOverlappedResult(port,&packet->operation,&packet->bytes,FALSE)?0:GetLastError();
  }
  if(error!=ERROR_BUSY||GetTickCount()-start>=2000u)break;
  Sleep(10);
 }
 /* Queue acceptance only. Release the shared owner before a client starts. */
 if(!error)Sleep(150);CloseHandle(port);
 done:
 /* An uncancelled request may still touch OVERLAPPED/event storage. Retain
  * this single packet until process exit and stop further UART submissions. */
 if(!pending){CloseHandle(packet->operation.hEvent);HeapFree(GetProcessHeap(),0,packet);}
 return error;
}
static void emit_record(unsigned id,unsigned event,DWORD code,int to_uart)
{
 char line[96],*p=line;DWORD got=0,error;
 p=text(p,"A04HW RUN=");p=hex(p,run_id);p=text(p," ID=");p=hex(p,id);
 p=text(p," EVT=");p=hex(p,event);p=text(p," CODE=");p=hex(p,code);*p++='\n';
 if(logfile!=INVALID_HANDLE_VALUE&&(!WriteFile(logfile,line,(DWORD)(p-line),&got,NULL)||got!=(DWORD)(p-line)||!FlushFileBuffers(logfile)))log_error=ERROR_WRITE_FAULT;
 if(to_uart){error=uart_record(line,(DWORD)(p-line));if(error)uart_error=error;}
}
static void record(unsigned id,unsigned event,DWORD code){emit_record(id,event,code,1);}
static DWORD local_system(void)
{
 HANDLE token;BYTE buffer[256];DWORD bytes,error=0;
 if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return GetLastError();
 if(!GetTokenInformation(token,TokenUser,buffer,sizeof(buffer),&bytes))error=GetLastError();
 else if(!IsWellKnownSid(((TOKEN_USER *)buffer)->User.Sid,WinLocalSystemSid))error=ERROR_ACCESS_DENIED;
 CloseHandle(token);return error;
}
static DWORD disable_self(void)
{
 SC_HANDLE manager,service;DWORD error=0;
 manager=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);if(!manager)return GetLastError();
 service=OpenServiceW(manager,service_name,SERVICE_CHANGE_CONFIG);
 if(!service)error=GetLastError();
 else {if(!ChangeServiceConfigW(service,SERVICE_NO_CHANGE,SERVICE_DISABLED,SERVICE_NO_CHANGE,NULL,NULL,NULL,NULL,NULL,NULL,NULL))error=GetLastError();CloseServiceHandle(service);}
 CloseServiceHandle(manager);return error;
}
static DWORD hold_directory(const WCHAR *name,HANDLE *held)
{
 BY_HANDLE_FILE_INFORMATION info;
 *held=CreateFileW(name,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
 if(*held==INVALID_HANDLE_VALUE)return GetLastError();
 if(!GetFileInformationByHandle(*held,&info)||!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))return ERROR_ACCESS_DENIED;
 return 0;
}
static DWORD paths(void)
{
 WCHAR system[MAX_PATH],module[MAX_PATH];DWORD n,error;
 n=GetSystemDirectoryW(system,MAX_PATH);if(!n||n>=MAX_PATH)return ERROR_INSUFFICIENT_BUFFER;
 error=path(base_directory,system,service_name);if(error)return error;
 error=path(executable,system,service_exe);if(error)return error;
 error=path(marker,base_directory,L"attempted.bin");if(error)return error;
 n=GetModuleFileNameW(NULL,module,MAX_PATH);if(!n||n>=MAX_PATH||lstrcmpiW(module,executable))return ERROR_BAD_PATHNAME;
#if A04_HARDWARE_PROBE_ENGINE_PCM
 error=hold_directory(base_directory,&base_handle);if(error)return error;
 error=path(directory,base_directory,L"clients");if(error)return error;
 error=path(log_directory,base_directory,L"logs");if(error)return error;
 error=hold_directory(log_directory,&log_directory_handle);if(error)return error;
#else
 if(lstrcpynW(directory,base_directory,MAX_PATH)!=directory || lstrcpynW(log_directory,base_directory,MAX_PATH)!=log_directory)return ERROR_BAD_PATHNAME;
#endif
 return hold_directory(directory,&directory_handle);
}
static DWORD open_log(void)
{
 WCHAR name[MAX_PATH];BY_HANDLE_FILE_INFORMATION info;DWORD error=path(name,log_directory,L"service.log");if(error)return error;
 logfile=CreateFileW(name,FILE_APPEND_DATA,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL);
 if(logfile==INVALID_HANDLE_VALUE)return GetLastError();
 if(!GetFileInformationByHandle(logfile,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))){CloseHandle(logfile);logfile=INVALID_HANDLE_VALUE;return ERROR_ACCESS_DENIED;}
 return 0;
}
static DWORD consume_attempt(void)
{
#if A04_HARDWARE_PROBE_ENGINE_PCM
 static const char content[]="MT8167EngineProbe attempted\n";
#else
 static const char content[]="MT8167HardwareProbe attempted\n";
#endif
 HANDLE file;DWORD got,error=0;
 file=CreateFileW(marker,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL);
 if(file==INVALID_HANDLE_VALUE)return GetLastError();
 if(!WriteFile(file,content,sizeof(content)-1,&got,NULL)||got!=sizeof(content)-1||!FlushFileBuffers(file))error=ERROR_WRITE_FAULT;
 CloseHandle(file);return error; /* Marker is never deleted, even on failure. */
}
static DWORD signed_image(const WCHAR *name,HANDLE *locked)
{
 WINTRUST_FILE_INFO file;WINTRUST_DATA trust;GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;
 BY_HANDLE_FILE_INFORMATION info;LONG result;HANDLE h;HMODULE library;
 typedef LONG (WINAPI *verify_fn)(HWND,GUID *,LPVOID);verify_fn verify;
 *locked=INVALID_HANDLE_VALUE;
 h=CreateFileW(name,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
 if(h==INVALID_HANDLE_VALUE)return GetLastError();
 if(!GetFileInformationByHandle(h,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||(!info.nFileSizeHigh&&!info.nFileSizeLow)){CloseHandle(h);return ERROR_ACCESS_DENIED;}
 library=LoadLibraryExW(L"wintrust.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!library){DWORD error=GetLastError();CloseHandle(h);return error;}
 verify=(verify_fn)GetProcAddress(library,"WinVerifyTrust");
 if(!verify){FreeLibrary(library);CloseHandle(h);return ERROR_PROC_NOT_FOUND;}
 memset(&file,0,sizeof(file));file.cbStruct=sizeof(file);file.pcwszFilePath=name;file.hFile=h;
 memset(&trust,0,sizeof(trust));trust.cbStruct=sizeof(trust);trust.dwUIChoice=WTD_UI_NONE;
 trust.fdwRevocationChecks=WTD_REVOKE_NONE;trust.dwUnionChoice=WTD_CHOICE_FILE;trust.pFile=&file;
 trust.dwStateAction=WTD_STATEACTION_VERIFY;trust.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL|WTD_REVOCATION_CHECK_NONE;
 result=verify((HWND)INVALID_HANDLE_VALUE,&action,&trust);
 trust.dwStateAction=WTD_STATEACTION_CLOSE;(void)verify((HWND)INVALID_HANDLE_VALUE,&action,&trust);FreeLibrary(library);
 if(result){CloseHandle(h);return (DWORD)result;}
 *locked=h;return 0; /* Deny write/delete through CreateProcess and child exit. */
}
#if A04_HARDWARE_PROBE_ENGINE_PCM
/* Fixed targets only; package selection is pinned by the private media builder.
 * No parent update or caller-supplied INF/ID/argument is accepted. */
static const struct {const WCHAR *id,*folder,*inf,*sys,*cat;} pnp_targets[3]={
 {L"MT8167\\AFE",L"audio",L"mt8167-afe-engine-pcm.inf",L"mt8167-audio.sys",L"mt8167-afe-engine-pcm.cat"},
 {L"MT8167\\USB20",L"usb",L"mt8167-usb20.inf",L"mt8167-usb20.sys",L"mt8167-usb20.cat"},
 {L"A04BR3\\MT6630SDIO",L"wifi",L"mt6630-runtime.inf",L"mt6630-runtime.sys",L"mt6630-runtime.cat"}
};
typedef BOOL (WINAPI *pnp_update_fn)(HWND,LPCWSTR,LPCWSTR,DWORD,PBOOL);
typedef CONFIGRET (WINAPI *pnp_size_fn)(PULONG,PCWSTR,ULONG);
typedef CONFIGRET (WINAPI *pnp_list_fn)(PCWSTR,PWSTR,ULONG,ULONG);
typedef CONFIGRET (WINAPI *pnp_locate_fn)(PDEVINST,DEVINSTID_W,ULONG);
typedef CONFIGRET (WINAPI *pnp_status_fn)(PULONG,PULONG,DEVINST,ULONG);
typedef CONFIGRET (WINAPI *pnp_property_fn)(DEVINST,ULONG,PULONG,PVOID,PULONG,ULONG);
struct pnp_apis {
 HMODULE newdev,cm;pnp_update_fn update;pnp_size_fn size;pnp_list_fn list;
 pnp_locate_fn locate;pnp_status_fn status;pnp_property_fn property;
};
static void pnp_record(unsigned index,unsigned event,DWORD value)
{emit_record(10+index,event,value,0); /* Disk authoritative; UART optional. */}
static DWORD pnp_load(struct pnp_apis *a)
{
 a->newdev=LoadLibraryExW(L"newdev.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!a->newdev)return GetLastError();
 a->cm=LoadLibraryExW(L"cfgmgr32.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!a->cm)return GetLastError();
 a->update=(pnp_update_fn)GetProcAddress(a->newdev,"UpdateDriverForPlugAndPlayDevicesW");
 a->size=(pnp_size_fn)GetProcAddress(a->cm,"CM_Get_Device_ID_List_SizeW");
 a->list=(pnp_list_fn)GetProcAddress(a->cm,"CM_Get_Device_ID_ListW");
 a->locate=(pnp_locate_fn)GetProcAddress(a->cm,"CM_Locate_DevNodeW");
 a->status=(pnp_status_fn)GetProcAddress(a->cm,"CM_Get_DevNode_Status");
 a->property=(pnp_property_fn)GetProcAddress(a->cm,"CM_Get_DevNode_Registry_PropertyW");
 return a->update&&a->size&&a->list&&a->locate&&a->status&&a->property?0:ERROR_PROC_NOT_FOUND;
}
static DWORD pnp_observe(struct pnp_apis *a,unsigned index,unsigned phase)
{
 ULONG length=0,offset=0,matches=0,started=0;WCHAR *ids;CONFIGRET cr;DWORD error=0;
 pnp_record(index,phase,0);
 /* Present list plus exact HWID match avoids guessing PDO instance suffixes. */
 cr=a->size(&length,NULL,CM_GETIDLIST_FILTER_PRESENT);pnp_record(index,EV_PNP_LIST_CR,cr);
 if(cr!=CR_SUCCESS)return ERROR_NOT_READY;
 if(!length||length>524288u)return ERROR_INVALID_DATA;
 ids=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(SIZE_T)length*sizeof(WCHAR));
 if(!ids)return ERROR_NOT_ENOUGH_MEMORY;
 cr=a->list(NULL,ids,length,CM_GETIDLIST_FILTER_PRESENT);pnp_record(index,EV_PNP_LIST_CR,cr);
 if(cr!=CR_SUCCESS){error=ERROR_NOT_READY;goto done;}
 if(ids[length-1]){error=ERROR_INVALID_DATA;goto done;}
 while(offset<length&&ids[offset]){
  DEVINST node;ULONG n=offset,type=0,bytes,flags=0,problem=0,j;WCHAR hardware[512];int match=0;
  while(n<length&&ids[n])n++;
  if(n>=length){error=ERROR_INVALID_DATA;break;}
  cr=a->locate(&node,ids+offset,CM_LOCATE_DEVNODE_NORMAL);offset=n+1;
  if(cr!=CR_SUCCESS){pnp_record(index,EV_PNP_LOCATE_CR,cr);error=ERROR_NOT_READY;continue;}
  memset(hardware,0,sizeof(hardware));bytes=sizeof(hardware);
  cr=a->property(node,CM_DRP_HARDWAREID,&type,hardware,&bytes,0);
  if(cr==CR_NO_SUCH_VALUE)continue;
  if(cr!=CR_SUCCESS){pnp_record(index,EV_PNP_PROPERTY_CR,cr);error=ERROR_NOT_READY;continue;}
  if(type!=REG_MULTI_SZ||bytes<4||bytes>sizeof(hardware)||(bytes&1)||hardware[bytes/2-1]||hardware[bytes/2-2]){
   pnp_record(index,EV_PNP_PROPERTY_CR,CR_INVALID_DATA);error=ERROR_INVALID_DATA;continue;
  }
  for(j=0;j<bytes/2&&hardware[j];){
   if(!lstrcmpiW(hardware+j,pnp_targets[index].id))match=1;
   while(j<bytes/2&&hardware[j])j++;j++;
  }
  if(!match)continue;
  matches++;cr=a->status(&flags,&problem,node,0);
  pnp_record(index,EV_PNP_STATUS_CR,cr);pnp_record(index,EV_PNP_STATUS,flags);pnp_record(index,EV_PNP_PROBLEM,problem);
  if(cr==CR_SUCCESS&&(flags&DN_STARTED)&&!(flags&DN_HAS_PROBLEM)&&!problem)started++;
  else error=ERROR_NOT_READY;
 }
 if(!matches)error=ERROR_NOT_FOUND;
 done:
 pnp_record(index,EV_PNP_MATCHES,matches);pnp_record(index,EV_PNP_STARTED,started);
 HeapFree(GetProcessHeap(),0,ids);return error;
}
static DWORD pnp_hold(unsigned index,const WCHAR *drivers,WCHAR inf[MAX_PATH],HANDLE held[4])
{
 WCHAR folder[MAX_PATH],full[MAX_PATH];BY_HANDLE_FILE_INFORMATION info;unsigned i;DWORD error;
 const WCHAR *names[3]={pnp_targets[index].inf,pnp_targets[index].sys,pnp_targets[index].cat};
 error=path(folder,drivers,pnp_targets[index].folder);if(error)return error;
 error=hold_directory(folder,&held[0]);if(error)return error;
 for(i=0;i<3;i++){
  error=path(full,folder,names[i]);if(error)return error;
  if(i==2)error=signed_image(full,&held[i+1]);
  else {
   held[i+1]=CreateFileW(full,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
   error=held[i+1]==INVALID_HANDLE_VALUE?GetLastError():0;
   if(!error&&(!GetFileInformationByHandle(held[i+1],&info)||
      (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
      (!info.nFileSizeHigh&&!info.nFileSizeLow)))error=ERROR_INVALID_DATA;
  }
  if(error)return error;
 }
 return path(inf,folder,pnp_targets[index].inf);
}
static DWORD pnp_once(int *halt)
{
 struct pnp_apis a;WCHAR drivers[MAX_PATH],infs[3][MAX_PATH];
 HANDLE root=INVALID_HANDLE_VALUE,held[3][4];unsigned i,j;DWORD error,result=0,observed;BOOL success,reboot;
 memset(&a,0,sizeof(a));for(i=0;i<3;i++)for(j=0;j<4;j++)held[i][j]=INVALID_HANDLE_VALUE;
 *halt=1;error=pnp_load(&a);if(error){result=error;goto done;}
 error=path(drivers,base_directory,L"drivers");if(!error)error=hold_directory(drivers,&root);
 if(error){result=error;goto done;}
 /* All CAT signatures and non-reparse paths are checked before any update.
  * Locks deny write/delete; Windows checks INF/SYS catalog membership. */
 for(i=0;i<3;i++){
  error=pnp_hold(i,drivers,infs[i],held[i]);pnp_record(i,EV_PNP_PACKAGE,error);
  if(error){result=error;goto done;}
 }
 for(i=0;i<3;i++){
  (void)pnp_observe(&a,i,EV_PNP_BEFORE);
  if(log_error){result=log_error;goto done;}
  if(WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0){result=ERROR_CANCELLED;goto done;}
  pnp_record(i,EV_PNP_API_BEGIN,INSTALLFLAG_FORCE|INSTALLFLAG_NONINTERACTIVE);
  if(log_error){result=log_error;goto done;}
  reboot=FALSE;SetLastError(0);
  success=a.update(NULL,pnp_targets[i].id,infs[i],INSTALLFLAG_FORCE|INSTALLFLAG_NONINTERACTIVE,&reboot);
  error=success?0:GetLastError();if(!success&&!error)error=ERROR_GEN_FAILURE;
  pnp_record(i,EV_PNP_API_SUCCESS,success);pnp_record(i,EV_PNP_WIN32,error);pnp_record(i,EV_PNP_REBOOT,reboot);
  observed=pnp_observe(&a,i,EV_PNP_AFTER);
  if(!error)error=observed;
  pnp_record(i,EV_PNP_RESULT,reboot?ERROR_SUCCESS_REBOOT_REQUIRED:error);
  if(reboot||error==ERROR_SUCCESS_REBOOT_REQUIRED){result=ERROR_SUCCESS_REBOOT_REQUIRED;goto done;}
  if(error&&!result)result=error;
 }
 *halt=0; /* Keep failures; permit independent WLAN/audio diagnostics. */
 done:
 for(i=0;i<3;i++)for(j=0;j<4;j++)if(held[i][j]!=INVALID_HANDLE_VALUE)CloseHandle(held[i][j]);
 if(root!=INVALID_HANDLE_VALUE)CloseHandle(root);
 if(a.cm)FreeLibrary(a.cm);if(a.newdev)FreeLibrary(a.newdev);
 emit_record(0,EV_PNP_RESULT,result,0);return result;
}
#endif
static HANDLE child_log(const WCHAR *name,SECURITY_ATTRIBUTES *sa)
{
 WCHAR full[MAX_PATH];if(path(full,log_directory,name)){SetLastError(ERROR_INSUFFICIENT_BUFFER);return INVALID_HANDLE_VALUE;}
 return CreateFileW(full,GENERIC_WRITE,FILE_SHARE_READ,sa,CREATE_NEW,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL);
}
static DWORD run_client(unsigned index)
{
 WCHAR image[MAX_PATH],command[MAX_PATH+3];HANDLE image_lock=INVALID_HANDLE_VALUE;
 HANDLE output=INVALID_HANDLE_VALUE,error_output=INVALID_HANDLE_VALUE,input=INVALID_HANDLE_VALUE,job=NULL;
 SECURITY_ATTRIBUTES sa;STARTUPINFOEXW startup;PROCESS_INFORMATION child;JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit;
 LPPROC_THREAD_ATTRIBUTE_LIST attributes=NULL;SIZE_T bytes=0;HANDLE inherit[3],waits[2];DWORD error=0,wait,exit_code=0,got;unsigned n;
 int created=0,resumed=0,finished=0;
 if(index>=2)return ERROR_INVALID_PARAMETER;
 error=path(image,directory,clients[index]);if(error)return error;
 error=signed_image(image,&image_lock);record(index+1,EV_SIGNATURE,error);if(error)goto done;
 memset(&sa,0,sizeof(sa));sa.nLength=sizeof(sa);sa.bInheritHandle=TRUE;
 output=child_log(stdout_names[index],&sa);if(output==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 error_output=child_log(stderr_names[index],&sa);if(error_output==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,NULL);
 if(input==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 job=CreateJobObjectW(NULL,NULL);if(!job){error=GetLastError();goto done;}
 memset(&limit,0,sizeof(limit));limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
 if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limit,sizeof(limit))){error=GetLastError();goto done;}
 InitializeProcThreadAttributeList(NULL,1,0,&bytes);
 if(!bytes){error=GetLastError();goto done;}
 attributes=HeapAlloc(GetProcessHeap(),0,bytes);if(!attributes){error=ERROR_NOT_ENOUGH_MEMORY;goto done;}
 if(!InitializeProcThreadAttributeList(attributes,1,0,&bytes)){error=GetLastError();HeapFree(GetProcessHeap(),0,attributes);attributes=NULL;goto done;}
 inherit[0]=input;inherit[1]=output;inherit[2]=error_output;
 if(!UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherit,sizeof(inherit),NULL,NULL)){error=GetLastError();goto done;}
 memset(&startup,0,sizeof(startup));startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
 startup.StartupInfo.hStdInput=input;startup.StartupInfo.hStdOutput=output;startup.StartupInfo.hStdError=error_output;startup.lpAttributeList=attributes;
 command[0]=L'"';for(n=0;image[n];n++)command[n+1]=image[n];command[n+1]=L'"';command[n+2]=0;
 memset(&child,0,sizeof(child));
 if(log_error){error=log_error;goto done;}
 if(WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0){error=ERROR_CANCELLED;goto done;}
 /* Both probes currently take NO arguments (ae1c716 and Native WLAN source).
  * Never use cmd.exe, a path parameter, PATH lookup or service-start args. */
 if(!CreateProcessW(image,command,NULL,NULL,TRUE,CREATE_SUSPENDED|CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,NULL,directory,&startup.StartupInfo,&child)){error=GetLastError();goto done;}
 created=1;
 if(!AssignProcessToJobObject(job,child.hProcess)){error=GetLastError();goto done;}
 if(ResumeThread(child.hThread)==(DWORD)-1){error=GetLastError();goto done;}resumed=1;
 record(index+1,EV_START,0);waits[0]=child.hProcess;waits[1]=stop_event;
 wait=WaitForMultipleObjects(2,waits,FALSE,limits[index]);
 if(wait==WAIT_OBJECT_0){finished=1;if(!GetExitCodeProcess(child.hProcess,&exit_code))error=GetLastError();else error=exit_code;record(index+1,EV_EXIT,error);}
 else {error=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:wait==WAIT_OBJECT_0+1?ERROR_CANCELLED:GetLastError();record(index+1,wait==WAIT_TIMEOUT?EV_TIMEOUT:EV_CANCEL,error);}
done:
 if(created&&!finished){
  BOOL stopped=resumed?TerminateJobObject(job,error?error:ERROR_PROCESS_ABORTED):TerminateProcess(child.hProcess,error?error:ERROR_PROCESS_ABORTED);
  got=stopped?0:GetLastError();record(index+1,EV_TERMINATE,got);
  if(stopped){got=WaitForSingleObject(child.hProcess,5000);record(index+1,EV_TERMINATE,got);if(got!=WAIT_OBJECT_0)child_uncertain=1;}
  else child_uncertain=1;
 }
 if(created){CloseHandle(child.hThread);CloseHandle(child.hProcess);}
 if(attributes){DeleteProcThreadAttributeList(attributes);HeapFree(GetProcessHeap(),0,attributes);}
 if(job)CloseHandle(job);
 if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
 if(output!=INVALID_HANDLE_VALUE){if(!FlushFileBuffers(output)){record(index+1,EV_LOG_ERROR,GetLastError());if(!error)error=ERROR_WRITE_FAULT;}CloseHandle(output);}
 if(error_output!=INVALID_HANDLE_VALUE){if(!FlushFileBuffers(error_output)){record(index+1,EV_LOG_ERROR,GetLastError());if(!error)error=ERROR_WRITE_FAULT;}CloseHandle(error_output);}
 if(image_lock!=INVALID_HANDLE_VALUE)CloseHandle(image_lock);
 if(!created||!resumed)record(index+1,EV_ERROR,error);
 return error;
}
static DWORD once(void)
{
 DWORD error,result=0,cleanup_error;unsigned i;HANDLE power=INVALID_HANDLE_VALUE;REASON_CONTEXT reason;
 error=local_system();if(error)return error;
 error=disable_self();if(error){record(0,EV_DISABLED,error);return error;}
 error=paths();if(error)return error;
 error=open_log();if(error)return error;
 record(0,EV_DISABLED,0);record(0,EV_BEGIN,0);record(0,EV_MODE,A04_HARDWARE_PROBE_ENGINE_PCM);error=consume_attempt();record(0,EV_ATTEMPTED,error);
 if(error||log_error)return error?error:log_error;
 /* Windows 7+ desktop API, also used by this native RT8.1 service.
  * SystemRequired only: explicit user Sleep is still honored by Windows. */
 memset(&reason,0,sizeof(reason));reason.Version=POWER_REQUEST_CONTEXT_VERSION;
 reason.Flags=POWER_REQUEST_CONTEXT_SIMPLE_STRING;
 reason.Reason.SimpleReasonString=L"MT8167 WLAN・音声の一回限り試験";
 power=PowerCreateRequest(&reason);
 if(power==INVALID_HANDLE_VALUE){result=GetLastError();record(0,EV_POWER_SET,result);return result;}
 if(!PowerSetRequest(power,PowerRequestSystemRequired)){
  result=GetLastError();record(0,EV_POWER_SET,result);goto close_power;
 }
 record(0,EV_POWER_SET,0);
#if A04_HARDWARE_PROBE_ENGINE_PCM
 {int halt;result=pnp_once(&halt);if(halt)goto clear_power;}
#endif
 for(i=0;i<2;i++){
  if(WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0){if(!result)result=ERROR_CANCELLED;break;}
  error=run_client(i);if(error&&!result)result=error;if(child_uncertain)break;
 }
 /* Normal completion, timeout, signature/start failure and SCM STOP all
  * converge here. Never reassert a request after explicit user Sleep. */
#if A04_HARDWARE_PROBE_ENGINE_PCM
clear_power:
#endif
 cleanup_error=PowerClearRequest(power,PowerRequestSystemRequired)?0:GetLastError();
 record(0,EV_POWER_CLEAR,cleanup_error);if(cleanup_error&&!result)result=cleanup_error;
close_power:
 cleanup_error=CloseHandle(power)?0:GetLastError();power=INVALID_HANDLE_VALUE;
 record(0,EV_POWER_CLOSE,cleanup_error);if(cleanup_error&&!result)result=cleanup_error;
 if(log_error&&!result)result=log_error;
 return result;
}
static void WINAPI control(DWORD code)
{if(code==SERVICE_CONTROL_STOP&&stop_event)SetEvent(stop_event);}
static void WINAPI service_main(DWORD argc,WCHAR **argv)
{
 DWORD error,event_error;(void)argc;(void)argv;
 stop_event=CreateEventW(NULL,TRUE,FALSE,NULL);event_error=stop_event?0:GetLastError();
 status_handle=RegisterServiceCtrlHandlerW(service_name,control);
 if(!status_handle){(void)disable_self();return;}
 memset(&service_status,0,sizeof(service_status));service_status.dwServiceType=SERVICE_WIN32_OWN_PROCESS;
 service_status.dwCurrentState=SERVICE_RUNNING;service_status.dwControlsAccepted=SERVICE_ACCEPT_STOP;
 if(!SetServiceStatus(status_handle,&service_status)){(void)disable_self();return;}
 if(!stop_event)(void)disable_self();
 error=stop_event?once():event_error;record(0,EV_UART,uart_error);
 #if !A04_HARDWARE_PROBE_ENGINE_PCM
 if(!error&&uart_error)error=uart_error;
#endif
 record(0,EV_END,error);
 #if !A04_HARDWARE_PROBE_ENGINE_PCM
 if(!error&&uart_error)error=uart_error;
#endif
 if(!error&&log_error)error=log_error;emit_record(0,EV_FINAL,error,0);
 if(!error&&log_error)error=log_error;
 service_status.dwCurrentState=SERVICE_STOPPED;service_status.dwControlsAccepted=0;
 if(error){service_status.dwWin32ExitCode=ERROR_SERVICE_SPECIFIC_ERROR;service_status.dwServiceSpecificExitCode=error;}
 SetServiceStatus(status_handle,&service_status);
 if(stop_event){CloseHandle(stop_event);stop_event=NULL;}
}
static DWORD register_service(void)
{
 SC_HANDLE manager,service;DWORD error;WCHAR command[MAX_PATH+3];unsigned n;
 SERVICE_FAILURE_ACTIONSW actions;
 error=paths();if(error)return error;
 if(GetFileAttributesW(marker)!=INVALID_FILE_ATTRIBUTES)return ERROR_ALREADY_EXISTS;
 error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND)return error;
 command[0]=L'"';for(n=0;executable[n];n++)command[n+1]=executable[n];command[n+1]=L'"';command[n+2]=0;
 manager=OpenSCManagerW(NULL,NULL,SC_MANAGER_CREATE_SERVICE);if(!manager)return GetLastError();
 service=CreateServiceW(manager,service_name,service_name,SERVICE_CHANGE_CONFIG,SERVICE_WIN32_OWN_PROCESS,SERVICE_AUTO_START,SERVICE_ERROR_NORMAL,command,NULL,NULL,NULL,L"LocalSystem",NULL);
 if(!service){error=GetLastError();CloseServiceHandle(manager);return error;}
 memset(&actions,0,sizeof(actions));
 error=0;
 if(!ChangeServiceConfig2W(service,SERVICE_CONFIG_FAILURE_ACTIONS,&actions)){
  error=GetLastError();(void)ChangeServiceConfigW(service,SERVICE_NO_CHANGE,SERVICE_DISABLED,SERVICE_NO_CHANGE,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
 }
 CloseServiceHandle(service);CloseServiceHandle(manager);return error;
}
static DWORD start_service(void)
{
 SC_HANDLE manager,service;DWORD error=0;
 manager=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);if(!manager)return GetLastError();
 service=OpenServiceW(manager,service_name,SERVICE_START);
 if(!service)error=GetLastError();else{if(!StartServiceW(service,0,NULL))error=GetLastError();CloseServiceHandle(service);}
 CloseServiceHandle(manager);return error;
}
static const WCHAR *arguments(void)
{
 const WCHAR *p=GetCommandLineW();if(*p==L'"'){p++;while(*p&&*p!=L'"')p++;if(*p)p++;}else while(*p&&*p!=L' '&&*p!=L'\t')p++;
 while(*p==L' '||*p==L'\t')p++;return p;
}
void mainCRTStartup(void)
{
 DWORD error=0;const WCHAR *args=arguments();run_id=GetTickCount();SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
 if(!lstrcmpW(args,L"--install"))error=register_service();
 else if(!lstrcmpW(args,L"--start"))error=start_service();
 else if(*args)error=ERROR_INVALID_PARAMETER;
 else {SERVICE_TABLE_ENTRYW table[2]={{(LPWSTR)service_name,service_main},{NULL,NULL}};if(!StartServiceCtrlDispatcherW(table))error=GetLastError();}
 if(logfile!=INVALID_HANDLE_VALUE)CloseHandle(logfile);
 if(directory_handle!=INVALID_HANDLE_VALUE)CloseHandle(directory_handle);
 if(log_directory_handle!=INVALID_HANDLE_VALUE)CloseHandle(log_directory_handle);
 if(base_handle!=INVALID_HANDLE_VALUE)CloseHandle(base_handle);
 ExitProcess(error);
}
