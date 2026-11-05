/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One fixed, guarded release operation; no shell input is accepted from UART. */
#include <windows.h>
#include <stddef.h>
/* Freestanding CRT entry: avoid any dependency on a target C runtime. */
void *memcpy(void *dst,const void *src,size_t n)
{ volatile unsigned char *d=dst; const volatile unsigned char *s=src; size_t i; for(i=0;i<n;i++)d[i]=s[i]; return dst; }
void *memset(void *dst,int value,size_t n)
{ volatile unsigned char *d=dst; size_t i; for(i=0;i<n;i++)d[i]=(unsigned char)value; return dst; }
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
static HANDLE open_uart(void)
{
    return CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
}
static void report(const char *message,DWORD length)
{
    DWORD start=GetTickCount(),got;
    while(GetTickCount()-start<30000u) {
        HANDLE h=open_uart();
        if(h!=INVALID_HANDLE_VALUE) {
            BOOL ok=DeviceIoControl(h,A04_UART_SCREEN_SEND,(void *)message,length,NULL,0,&got,NULL);
            CloseHandle(h); if(ok)return;
        }
        Sleep(100);
    }
}
static int run(void)
{
    DWORD request=0,got,rc; BOOL ok,ready=FALSE; HANDLE h;
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    WCHAR cmd[]=L"X:\\Windows\\System32\\cmd.exe /d /c X:\\a04br3\\release-toprgu-audit-volume.cmd";
    for(;;) {
        h=open_uart();
        if(h!=INVALID_HANDLE_VALUE) {
            got=0;ok=DeviceIoControl(h,A04_UART_VOLUME_GET,NULL,0,&request,sizeof(request),&got,NULL);
            CloseHandle(h);
            if(ok && got==sizeof(request) && !ready) {
                static const char msg[]="A04VOL_LISTENER_READY\n";
                report(msg,sizeof(msg)-1); ready=TRUE;
            }
            if(ok && got==sizeof(request) && request==1)break;
        }
        Sleep(200);
    }
    ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);ZeroMemory(&pi,sizeof(pi));
    if(!CreateProcessW(L"X:\\Windows\\System32\\cmd.exe",cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)) {
        report("A04VOL_LAUNCH_FAILED\n",21);return 2;
    }
    rc=WaitForSingleObject(pi.hProcess,120000);
    if(rc!=WAIT_OBJECT_0 || !GetExitCodeProcess(pi.hProcess,&rc)) {
        report("A04VOL_RELEASE_UNKNOWN\n",23);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return 3;
    }
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
    if(rc!=0) {report("A04VOL_RELEASE_FAILED\n",22);return 4;}
    report("A04VOL_RELEASED\n",16);return 0;
}

void mainCRTStartup(void) { ExitProcess((UINT)run()); }
