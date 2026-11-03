/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * WinPE-only reset/worker lifetime probe. The target read is held for two
 * seconds in the diagnostic driver; it performs no direct writes. */
#include <windows.h>
#include <winioctl.h>
#include <ntddstor.h>
#ifndef A04_SCRATCH_LBA
#error Select the same scratch LBA as the diagnostic miniport
#endif
static HANDLE ready;
static void say(const char *s) { DWORD n=0,w; while(s[n]) ++n; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,NULL); }
static DWORD WINAPI reader(void *unused) {
    HANDLE h; LARGE_INTEGER off; DWORD n=0; unsigned char *buf; BOOL ok;
    (void)unused;
    h=CreateFileW(L"\\\\.\\PhysicalDrive0",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                  NULL,OPEN_EXISTING,FILE_FLAG_NO_BUFFERING,NULL);
    buf=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(h==INVALID_HANDLE_VALUE || !buf) {
        if(buf) VirtualFree(buf,0,MEM_RELEASE);
        if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
        SetEvent(ready); return 2;
    }
    off.QuadPart=((LONGLONG)(A04_SCRATCH_LBA+16u))<<9;
    if(!SetFilePointerEx(h,off,NULL,FILE_BEGIN)) {
        VirtualFree(buf,0,MEM_RELEASE); CloseHandle(h); SetEvent(ready); return 3;
    }
    SetEvent(ready);
    ok=ReadFile(h,buf,4096,&n,NULL);
    VirtualFree(buf,0,MEM_RELEASE); CloseHandle(h);
    return ok && n==4096 ? 0 : 1;
}
void mainCRTStartup(void) {
    HANDLE h=INVALID_HANDLE_VALUE, thread=NULL; STORAGE_BUS_RESET_REQUEST req;
    DWORD n=0,wait,readerResult=0; BOOL reset;
    ready=CreateEventW(NULL,TRUE,FALSE,NULL);
    h=CreateFileW(L"\\\\.\\PhysicalDrive0",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                  NULL,OPEN_EXISTING,0,NULL);
    if(!ready || h==INVALID_HANDLE_VALUE) goto fail;
    thread=CreateThread(NULL,0,reader,NULL,0,NULL);
    if(!thread || WaitForSingleObject(ready,10000)!=WAIT_OBJECT_0) goto fail;
    Sleep(500);
    req.PathId=0;
    say("MSDC_RESET_IOCTL_STARTED\r\n");
    reset=DeviceIoControl(h,IOCTL_STORAGE_RESET_BUS,&req,sizeof(req),NULL,0,&n,NULL);
    say(reset ? "MSDC_RESET_IOCTL_SUCCESS\r\n" : "MSDC_RESET_IOCTL_FAILED\r\n");
    wait=WaitForSingleObject(thread,10000);
    if(wait!=WAIT_OBJECT_0) goto fail;
    if(!GetExitCodeThread(thread,&readerResult)) goto fail;
    if(readerResult>1) goto fail;
    say(readerResult ? "MSDC_RESET_READ_FAILED\r\n" : "MSDC_RESET_READ_SUCCEEDED\r\n");
    say("MSDC_RESET_PROBE_RETURNED\r\n");
    CloseHandle(thread); CloseHandle(h); CloseHandle(ready); ExitProcess(0);
fail:
    say("MSDC_RESET_PROBE_FAILED\r\n");
    if(thread) CloseHandle(thread);
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    if(ready) CloseHandle(ready);
    ExitProcess(1);
}
