/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * WinPE-only bounded fixed-size diagnostic, no scanout or disk writes. */
#include <windows.h>
#include "../../drivers/gpu/mt8167/ge8300_status.h"
static ULONG input[1024],output[1024];
static void say(const char *s) { DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,NULL); }
void mainCRTStartup(void)
{
    HANDLE h;DWORD n,i,pass,bytes;BOOL ok;
    h=CreateFileW(L"\\\\.\\A04GE8300",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE){say("VENDOR_COPY_OPEN_FAILED\r\n");ExitProcess(1);}
    for(pass=0;pass<3;pass++) {
        for(i=0;i<1024;i++){input[i]=(0x5a00c3a5u^(i*0x9e3779b1u))+(pass*0x01030507u);output[i]=0x69696969u;}
        bytes=0;ok=DeviceIoControl(h,A04_GE8300_IOCTL_VENDOR_COPY,input,sizeof(input),output,sizeof(output),&bytes,NULL);
        if(!ok || bytes!=sizeof(output)){say("VENDOR_COPY_IOCTL_FAILED\r\n");CloseHandle(h);ExitProcess(2);}
        n=0;for(i=0;i<1024;i++)if(input[i]!=output[i])++n;
        if(n){say("VENDOR_COPY_MISMATCH\r\n");CloseHandle(h);ExitProcess(3);}
        say("VENDOR_COPY_PASS\r\n");
    }
    CloseHandle(h);say("VENDOR_COPY_THREE_PASS\r\n");ExitProcess(0);
}
