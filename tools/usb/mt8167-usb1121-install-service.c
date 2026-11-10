// SPDX-License-Identifier: BSD-2-Clause-Patent
// Existing peripheral-update SCM/CreateProcess client pattern, fixed PS client only.
#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <reason.h>
#include <bcrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <winioctl.h>
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
#include "usb1121-service-pins.h"
static SERVICE_STATUS_HANDLE sh;
static SERVICE_STATUS ss;
static const WCHAR service[]=L"MT8167Usb1121Install198";
#include "usb1121-system-volume-flush.h"
#include "usb1121-service-uart.h"
static const WCHAR base[]=L"C:\\Windows\\System32\\MT8167Usb1121Install198";
static HANDLE hold(const WCHAR *p,const BYTE *want){
 HANDLE f=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
 BCRYPT_ALG_HANDLE a=NULL;BCRYPT_HASH_HANDLE h=NULL;BYTE buf[1024],sum[32];DWORD n,i;BY_HANDLE_FILE_INFORMATION info;int ok=0;
 if(f==INVALID_HANDLE_VALUE)return f;
 if(!GetFileInformationByHandle(f,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)))goto done;
 if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,NULL,0)<0||BCryptCreateHash(a,&h,NULL,0,NULL,0,0)<0)goto done;
 for(;;){if(!ReadFile(f,buf,sizeof(buf),&n,NULL))goto done;if(!n)break;if(BCryptHashData(h,buf,n,0)<0)goto done;}
 if(BCryptFinishHash(h,sum,32,0)<0)goto done;
 for(i=0;i<32;i++)if(sum[i]!=want[i])goto done;ok=1;
 done:if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);if(!ok){CloseHandle(f);f=INVALID_HANDLE_VALUE;}return f;
}
static DWORD trust(const WCHAR *p){WINTRUST_FILE_INFO f={0};WINTRUST_DATA d={0};GUID g=WINTRUST_ACTION_GENERIC_VERIFY_V2;LONG e;f.cbStruct=sizeof(f);f.pcwszFilePath=p;d.cbStruct=sizeof(d);d.dwUIChoice=WTD_UI_NONE;d.dwUnionChoice=WTD_CHOICE_FILE;d.pFile=&f;d.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;d.dwStateAction=WTD_STATEACTION_VERIFY;e=WinVerifyTrust(NULL,&g,&d);d.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(NULL,&g,&d);return e?ERROR_TRUST_FAILURE:0;}
static DWORD disable(void){SC_HANDLE m=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT),s;DWORD e;if(!m)return GetLastError();s=OpenServiceW(m,service,SERVICE_CHANGE_CONFIG);if(!s){e=GetLastError();CloseServiceHandle(m);return e;}e=ChangeServiceConfigW(s,SERVICE_NO_CHANGE,SERVICE_DISABLED,SERVICE_NO_CHANGE,NULL,NULL,NULL,NULL,NULL,NULL,NULL)?0:GetLastError();CloseServiceHandle(s);CloseServiceHandle(m);return e;}
static DWORD run(void){
 WCHAR ps[MAX_PATH]=L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
 WCHAR script[MAX_PATH]=L"C:\\Windows\\System32\\MT8167Usb1121Install198\\run-usb1121-service.ps1";
 WCHAR profile[MAX_PATH]=L"C:\\Windows\\System32\\MT8167Usb1121Install198\\profile.json";
 WCHAR cmd[1024]=L"\"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe\" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"C:\\Windows\\System32\\MT8167Usb1121Install198\\run-usb1121-service.ps1\"";
 HANDLE a=INVALID_HANDLE_VALUE,b=INVALID_HANDLE_VALUE;STARTUPINFOW si={0};PROCESS_INFORMATION pi={0};DWORD e=0,code=1;
 a=hold(script,launcher_hash);b=hold(profile,profile_hash);if(a==INVALID_HANDLE_VALUE||b==INVALID_HANDLE_VALUE)e=ERROR_CRC;
 if(!e)e=trust(ps);
 si.cb=sizeof(si);
 if(!e){if(!CreateProcessW(ps,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,base,&si,&pi))e=GetLastError();else{CloseHandle(pi.hThread);if(WaitForSingleObject(pi.hProcess,INFINITE)!=WAIT_OBJECT_0||!GetExitCodeProcess(pi.hProcess,&code))e=GetLastError();else e=code;CloseHandle(pi.hProcess);}}
 if(a!=INVALID_HANDLE_VALUE)CloseHandle(a);if(b!=INVALID_HANDLE_VALUE)CloseHandle(b);return e;
}
static DWORD reboot(void){HANDLE token;TOKEN_PRIVILEGES p={0};DWORD e;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token))return GetLastError();p.PrivilegeCount=1;if(!LookupPrivilegeValueW(NULL,SE_SHUTDOWN_NAME,&p.Privileges[0].Luid)){e=GetLastError();CloseHandle(token);return e;}p.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;SetLastError(0);if(!AdjustTokenPrivileges(token,FALSE,&p,0,NULL,NULL)){e=GetLastError();CloseHandle(token);return e;}e=GetLastError();CloseHandle(token);if(e)return e;return InitiateSystemShutdownExW(NULL,L"MediaTek MT8167 USB ABI3 PnP requested reboot",30,FALSE,TRUE,SHTDN_REASON_MAJOR_APPLICATION|SHTDN_REASON_FLAG_PLANNED)?0:GetLastError();}
static void WINAPI control(DWORD c){(void)c;}
/* Same separate post-SCM process pattern as peripheral-batch-reused.h.
 * No disk/UART work follows SERVICE_STOPPED in the SCM callback. */
static DWORD wait_for_stopped(void){
 SC_HANDLE m=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT),h;SERVICE_STATUS_PROCESS state;DWORD n,e=0,start=GetTickCount();
 if(!m)return GetLastError();h=OpenServiceW(m,service,SERVICE_QUERY_STATUS);if(!h){e=GetLastError();CloseServiceHandle(m);return e;}
 for(;;){if(!QueryServiceStatusEx(h,SC_STATUS_PROCESS_INFO,(BYTE *)&state,sizeof(state),&n)){e=GetLastError();break;}if(state.dwCurrentState==SERVICE_STOPPED)break;if(state.dwProcessId!=uart_pid){e=ERROR_INVALID_DATA;break;}if(GetTickCount()-start>=30000u){e=ERROR_TIMEOUT;break;}Sleep(250);}
 CloseServiceHandle(h);CloseServiceHandle(m);return e;
}
#define U198_MAX_PLANNED_REBOOTS 4u
static DWORD next_reboot_count(DWORD count,DWORD *next){if(count>=U198_MAX_PLANNED_REBOOTS)return ERROR_RETRY;*next=count+1;return 0;}
static DWORD reserve_reboot(void){
 DWORD data[3]={0x38393155u,198u,0},n,e=0,next;LARGE_INTEGER size;BY_HANDLE_FILE_INFORMATION info;
 HANDLE f=CreateFileW(L"C:\\Windows\\System32\\MT8167Usb1121Install198\\logs\\planned-reboots.bin",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL);
 if(f==INVALID_HANDLE_VALUE)return GetLastError();
 if(!GetFileInformationByHandle(f,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))||!GetFileSizeEx(f,&size)){e=ERROR_INVALID_DATA;goto done;}
 if(size.QuadPart){if(size.QuadPart!=sizeof(data)||!ReadFile(f,data,sizeof(data),&n,NULL)||n!=sizeof(data)||data[0]!=0x38393155u||data[1]!=198u){e=ERROR_INVALID_DATA;goto done;}}
 e=next_reboot_count(data[2],&next);if(e)goto done;data[2]=next;SetLastError(0);
 if(SetFilePointer(f,0,NULL,FILE_BEGIN)==INVALID_SET_FILE_POINTER&&GetLastError()){e=GetLastError();goto done;}
 if(!WriteFile(f,data,sizeof(data),&n,NULL)||n!=sizeof(data)||!FlushFileBuffers(f))e=ERROR_WRITE_FAULT;
 done:if(!CloseHandle(f)&&!e)e=GetLastError();return e;
}
static DWORD terminal_observer(DWORD result){
 DWORD e=wait_for_stopped(),d,flushed;
 if(e){(void)uart_only(U198_SCM_STOP,e);return e;}
 e=open_log();if(e){(void)uart_only(U198_SCM_STOP,e);return e;}record(U198_SCM_STOP,0);
 e=result;
 if(e==ERROR_SUCCESS_REBOOT_REQUIRED){d=reserve_reboot();if(d){e=d;record(U198_REBOOT_REQUEST,d);d=disable();if(d)e=d;}}
 flushed=close_log_and_flush_volume();if(flushed){(void)disable();return flushed;}
 if(e==ERROR_SUCCESS_REBOOT_REQUIRED){
  d=reboot();(void)uart_only(U198_REBOOT_REQUEST,d);
  if(d){e=d;d=disable();if(d)e=d;flushed=flush_fixed_system_volume();(void)uart_only(U198_VOLUME_FLUSH,flushed);if(flushed)return flushed;}
 }
 /* UART-observed final boundary. 3010 means wait Windows reboot/Resume. */
 (void)uart_only(U198_TERMINAL,e);return e;
}
static WCHAR *wide_hex(WCHAR *p,DWORD n){static const WCHAR h[]=L"0123456789ABCDEF";DWORD i;for(i=0;i<8;i++)*p++=h[(n>>(28-4*i))&15];return p;}
static DWORD observer_launch_flags(DWORD *flags){
 BOOL in_job=FALSE;DWORD e;
 if(!IsProcessInJob(GetCurrentProcess(),NULL,&in_job)){e=GetLastError();return e?e:ERROR_GEN_FAILURE;}
 *flags=CREATE_NO_WINDOW|CREATE_SUSPENDED;
 if(in_job)*flags|=CREATE_BREAKAWAY_FROM_JOB;
 return 0;
}
static DWORD start_observer(DWORD result){
 static const WCHAR exe[]=L"C:\\Windows\\System32\\MT8167Usb1121Install198.exe";
 WCHAR cmd[256],*p=cmd;STARTUPINFOW si={0};PROCESS_INFORMATION pi={0};BOOL child_in_job=TRUE;DWORD flags,e=trust(exe);
 if(e)return e;
 lstrcpyW(cmd,L"\"C:\\Windows\\System32\\MT8167Usb1121Install198.exe\" --uart-terminal ");p+=lstrlenW(cmd);
 p=wide_hex(p,uart_run);*p++=' ';p=wide_hex(p,uart_pid);*p++=' ';p=wide_hex(p,result);*p=0;si.cb=sizeof(si);
 e=observer_launch_flags(&flags);if(e)return e;
 if(!CreateProcessW(exe,cmd,NULL,NULL,FALSE,flags,NULL,base,&si,&pi)){e=GetLastError();return e?e:ERROR_GEN_FAILURE;}
 /* Check the suspended child before allowing any observer work. No fallback
  * creates a job-bound child when breakaway is denied or membership is unknown. */
 if(!IsProcessInJob(pi.hProcess,NULL,&child_in_job)){e=GetLastError();if(!e)e=ERROR_GEN_FAILURE;goto reject;}
 if(child_in_job){e=ERROR_ACCESS_DENIED;goto reject;}
 if(ResumeThread(pi.hThread)==(DWORD)-1){e=GetLastError();if(!e)e=ERROR_GEN_FAILURE;goto reject;}
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return 0;
 reject:(void)TerminateProcess(pi.hProcess,e);(void)WaitForSingleObject(pi.hProcess,5000);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return e;
}
static void WINAPI main_service(DWORD argc,WCHAR **argv){
 DWORD e,d;FILETIME now;(void)argc;(void)argv;
 uart_pid=GetCurrentProcessId();GetSystemTimeAsFileTime(&now);uart_run=now.dwLowDateTime^now.dwHighDateTime^uart_pid^GetTickCount();
 sh=RegisterServiceCtrlHandlerW(service,control);if(!sh){(void)uart_only(U198_RUN_RESULT,GetLastError());return;}
 ss.dwServiceType=SERVICE_WIN32_OWN_PROCESS;ss.dwCurrentState=SERVICE_RUNNING;ss.dwControlsAccepted=0;
 e=SetServiceStatus(sh,&ss)?0:GetLastError();if(!e)e=open_log();
 record(U198_START,e);if(!e)e=run();record(U198_RUN_RESULT,e);
 if(e==ERROR_SUCCESS_REBOOT_REQUIRED)record(U198_REBOOT_REQUIRED,e);else{d=disable();if(!e)e=d;}
 if(logfile!=INVALID_HANDLE_VALUE){if(!FlushFileBuffers(logfile))log_error=ERROR_WRITE_FAULT;if(!CloseHandle(logfile))log_error=ERROR_WRITE_FAULT;logfile=INVALID_HANDLE_VALUE;}
 if(log_error)e=log_error;
 d=start_observer(e);if(d){(void)uart_only(U198_SCM_STOP,d);(void)disable();e=d;}
 ss.dwCurrentState=SERVICE_STOPPED;ss.dwWin32ExitCode=e?ERROR_SERVICE_SPECIFIC_ERROR:0;ss.dwServiceSpecificExitCode=e;
 (void)SetServiceStatus(sh,&ss); /* Last operation in this callback. */
}
static DWORD parse_hex8(const WCHAR **arg,DWORD *value){DWORD i,n=0;for(i=0;i<8;i++){WCHAR c=*(*arg)++;DWORD x;if(c>='0'&&c<='9')x=c-'0';else if(c>='A'&&c<='F')x=c-'A'+10;else return ERROR_INVALID_PARAMETER;n=(n<<4)|x;}*value=n;return 0;}
void mainCRTStartup(void){
 const WCHAR *arg=GetCommandLineW();DWORD e,result;SERVICE_TABLE_ENTRYW t[2]={{(LPWSTR)service,main_service},{NULL,NULL}};
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
 if(*arg=='"'){arg++;while(*arg&&*arg!='"')arg++;if(*arg)arg++;}else while(*arg&&*arg!=' '&&*arg!='\t')arg++;while(*arg==' '||*arg=='\t')arg++;
 if(!*arg)e=StartServiceCtrlDispatcherW(t)?0:GetLastError();
 else if(lstrlenW(arg)>=16 && arg[0]=='-' && arg[1]=='-'){
  DWORD i;static const WCHAR prefix[]=L"--uart-terminal ";for(i=0;i<16;i++)if(arg[i]!=prefix[i])ExitProcess(ERROR_INVALID_PARAMETER);
  arg+=16;e=parse_hex8(&arg,&uart_run);if(!e&&*arg++!=' ')e=ERROR_INVALID_PARAMETER;if(!e)e=parse_hex8(&arg,&uart_pid);if(!e&&*arg++!=' ')e=ERROR_INVALID_PARAMETER;if(!e)e=parse_hex8(&arg,&result);if(!e&&*arg)e=ERROR_INVALID_PARAMETER;
  if(!e)e=terminal_observer(result);
 }else e=ERROR_INVALID_PARAMETER;
 ExitProcess(e);
}

void *memset(void *p,int v,size_t n){volatile unsigned char *q=p;while(n--)*q++=(unsigned char)v;return p;}
void *memcpy(void *p,const void *s,size_t n){volatile unsigned char *d=p;const volatile unsigned char *q=s;while(n--)*d++=*q++;return p;}
