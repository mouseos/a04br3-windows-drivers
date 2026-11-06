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
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"

static const WCHAR service_name[]=L"MT8167HardwareProbe";
static const WCHAR *const clients[2]={L"mt6630-wlan-probe.exe",L"mt8167-audio-first-playback.exe"};
static const WCHAR *const stdout_names[2]={L"wlan.stdout.txt",L"audio.stdout.txt"};
static const WCHAR *const stderr_names[2]={L"wlan.stderr.txt",L"audio.stderr.txt"};
static const DWORD limits[2]={600000u,120000u};
static WCHAR directory[MAX_PATH],executable[MAX_PATH],marker[MAX_PATH];
static HANDLE directory_handle=INVALID_HANDLE_VALUE,logfile=INVALID_HANDLE_VALUE,stop_event;
static SERVICE_STATUS_HANDLE status_handle;
static SERVICE_STATUS service_status;
static DWORD run_id,log_error,uart_error;
static int uart_quarantined,child_uncertain;
enum { EV_BEGIN=1,EV_DISABLED,EV_ATTEMPTED,EV_SIGNATURE,EV_START,EV_EXIT,
       EV_TIMEOUT,EV_CANCEL,EV_ERROR,EV_END,EV_TERMINATE,EV_LOG_ERROR,EV_UART,EV_FINAL };

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
 port=CreateFileW(L"\\.\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
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
static DWORD paths(void)
{
 WCHAR system[MAX_PATH],module[MAX_PATH];DWORD n,error;BY_HANDLE_FILE_INFORMATION info;
 n=GetSystemDirectoryW(system,MAX_PATH);if(!n||n>=MAX_PATH)return ERROR_INSUFFICIENT_BUFFER;
 error=path(directory,system,L"MT8167HardwareProbe");if(error)return error;
 error=path(executable,system,L"MT8167HardwareProbe.exe");if(error)return error;
 error=path(marker,directory,L"attempted.bin");if(error)return error;
 n=GetModuleFileNameW(NULL,module,MAX_PATH);if(!n||n>=MAX_PATH||lstrcmpiW(module,executable))return ERROR_BAD_PATHNAME;
 directory_handle=CreateFileW(directory,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
 if(directory_handle==INVALID_HANDLE_VALUE)return GetLastError();
 if(!GetFileInformationByHandle(directory_handle,&info)||!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))return ERROR_ACCESS_DENIED;
 return 0;
}
static DWORD open_log(void)
{
 WCHAR name[MAX_PATH];BY_HANDLE_FILE_INFORMATION info;DWORD error=path(name,directory,L"service.log");if(error)return error;
 logfile=CreateFileW(name,FILE_APPEND_DATA,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL);
 if(logfile==INVALID_HANDLE_VALUE)return GetLastError();
 if(!GetFileInformationByHandle(logfile,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))){CloseHandle(logfile);logfile=INVALID_HANDLE_VALUE;return ERROR_ACCESS_DENIED;}
 return 0;
}
static DWORD consume_attempt(void)
{
 static const char content[]="MT8167HardwareProbe attempted\n";HANDLE file;DWORD got,error=0;
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
static HANDLE child_log(const WCHAR *name,SECURITY_ATTRIBUTES *sa)
{
 WCHAR full[MAX_PATH];if(path(full,directory,name)){SetLastError(ERROR_INSUFFICIENT_BUFFER);return INVALID_HANDLE_VALUE;}
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
 DWORD error,result=0;unsigned i;
 error=local_system();if(error)return error;
 error=disable_self();if(error){record(0,EV_DISABLED,error);return error;}
 error=paths();if(error)return error;
 error=open_log();if(error)return error;
 record(0,EV_DISABLED,0);record(0,EV_BEGIN,0);error=consume_attempt();record(0,EV_ATTEMPTED,error);
 if(error||log_error)return error?error:log_error;
 for(i=0;i<2;i++){
  if(WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0){if(!result)result=ERROR_CANCELLED;break;}
  error=run_client(i);if(error&&!result)result=error;if(child_uncertain)break;
 }
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
 if(!error&&uart_error)error=uart_error;record(0,EV_END,error);
 if(!error&&uart_error)error=uart_error;
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
 ExitProcess(error);
}
