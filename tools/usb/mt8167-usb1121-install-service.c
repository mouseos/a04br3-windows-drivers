// SPDX-License-Identifier: BSD-2-Clause-Patent
// Existing peripheral-update SCM/CreateProcess client pattern, fixed PS client only.
#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include "usb1121-service-pins.h"
static SERVICE_STATUS_HANDLE sh;
static SERVICE_STATUS ss;
static const WCHAR service[]=L"MT8167Usb1121Install198";
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
static DWORD reboot(void){HANDLE token;TOKEN_PRIVILEGES p={0};DWORD e;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token))return GetLastError();p.PrivilegeCount=1;if(!LookupPrivilegeValueW(NULL,SE_SHUTDOWN_NAME,&p.Privileges[0].Luid)){e=GetLastError();CloseHandle(token);return e;}p.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;SetLastError(0);if(!AdjustTokenPrivileges(token,FALSE,&p,0,NULL,NULL)){e=GetLastError();CloseHandle(token);return e;}e=GetLastError();CloseHandle(token);if(e)return e;return InitiateSystemShutdownExW(NULL,L"MediaTek MT8167 USB ABI3 PnP requested reboot",10,FALSE,TRUE,SHTDN_REASON_MAJOR_APPLICATION|SHTDN_REASON_FLAG_PLANNED)?0:GetLastError();}
static void WINAPI control(DWORD c){(void)c;}
static void WINAPI main_service(DWORD argc,WCHAR **argv){DWORD e,d;(void)argc;(void)argv;sh=RegisterServiceCtrlHandlerW(service,control);if(!sh)return;ss.dwServiceType=SERVICE_WIN32_OWN_PROCESS;ss.dwCurrentState=SERVICE_RUNNING;ss.dwControlsAccepted=0;SetServiceStatus(sh,&ss);e=run();if(e==ERROR_SUCCESS_REBOOT_REQUIRED){e=reboot();if(e)disable();}else{d=disable();if(!e)e=d;}ss.dwCurrentState=SERVICE_STOPPED;ss.dwWin32ExitCode=e?ERROR_SERVICE_SPECIFIC_ERROR:0;ss.dwServiceSpecificExitCode=e;SetServiceStatus(sh,&ss);}
void mainCRTStartup(void){SERVICE_TABLE_ENTRYW t[2]={{(LPWSTR)service,main_service},{NULL,NULL}};SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);ExitProcess(StartServiceCtrlDispatcherW(t)?0:GetLastError());}

void *memset(void *p,int v,size_t n){volatile unsigned char *q=p;while(n--)*q++=(unsigned char)v;return p;}
void *memcpy(void *p,const void *s,size_t n){volatile unsigned char *d=p;const unsigned char *q=s;while(n--)*d++=*q++;return p;}
