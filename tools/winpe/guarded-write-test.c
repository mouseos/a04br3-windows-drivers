/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Test-only WinPE program. A host backup and recovery path are required. */
#include <windows.h>
#include <winioctl.h>
#include "gpt_guard.h"

static void say(const char *s)
{
    DWORD n=0, written;
    while (s[n]) ++n;
    (void)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&written,NULL);
}
static int sector(HANDLE h, void *buf, uint32_t lba, int write)
{
    LARGE_INTEGER offset; DWORD done=0;
    offset.QuadPart=(LONGLONG)lba*512;
    if (!SetFilePointerEx(h,offset,NULL,FILE_BEGIN)) return 0;
    if (write) {
        if (!WriteFile(h,buf,512,&done,NULL) || done!=512) return 0;
        return FlushFileBuffers(h)!=0;
    }
    return ReadFile(h,buf,512,&done,NULL) && done==512;
}
void mainCRTStartup(void)
{
    GET_LENGTH_INFORMATION length; DWORD got=0; uint32_t first,last,i;
    struct a04_gpt_guard g;
    unsigned char *buf=(unsigned char *)VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    HANDLE h=INVALID_HANDLE_VALUE;
    if (!buf) { say("GUARD_TEST allocation failed\r\n"); ExitProcess(1); }
    h=CreateFileW(L"\\\\.\\PhysicalDrive0",GENERIC_READ|GENERIC_WRITE,
                  FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_NO_BUFFERING,NULL);
    if (h==INVALID_HANDLE_VALUE ||
        !DeviceIoControl(h,IOCTL_DISK_GET_LENGTH_INFO,NULL,0,&length,sizeof(length),&got,NULL) ||
        length.Length.QuadPart!=(LONGLONG)30535680*512 ||
        !sector(h,buf,1,0) || !a04_gpt_guard_begin(&g,buf,30535680)) {
        say("GUARD_TEST device or GPT header rejected\r\n"); ExitProcess(1);
    }
    for (i=2;i<34;++i)
        if (!sector(h,buf,i,0) || !a04_gpt_guard_feed(&g,buf)) {
            say("GUARD_TEST GPT entries rejected\r\n"); ExitProcess(1);
        }
    if (!a04_gpt_guard_finish(&g,&first,&last) || first!=917504 ||
        !a04_gpt_guard_range(first,last,first,1) || !sector(h,buf,first,0)) {
        say("GUARD_TEST target rejected\r\n"); ExitProcess(1);
    }
    for (i=0;i<512;++i) if (buf[i]) {
        say("GUARD_TEST original is not zero\r\n"); ExitProcess(1);
    }
    for (i=0;i<512;++i) buf[i]=(unsigned char)(0xa5u^(i&0xffu));
    say("GUARD_TEST writing one saved sector\r\n");
    if (!sector(h,buf,first,1) || !sector(h,buf,first,0)) {
        say("GUARD_TEST failed after start; host recovery required\r\n"); ExitProcess(2);
    }
    for (i=0;i<512;++i) if (buf[i]!=(unsigned char)(0xa5u^(i&0xffu))) {
        say("GUARD_TEST pattern mismatch; host recovery required\r\n"); ExitProcess(2);
    }
    for (i=0;i<512;++i) buf[i]=0;
    if (!sector(h,buf,first,1) || !sector(h,buf,first,0)) {
        say("GUARD_TEST restore failed; host recovery required\r\n"); ExitProcess(2);
    }
    for (i=0;i<512;++i) if (buf[i]) {
        say("GUARD_TEST restore mismatch; host recovery required\r\n"); ExitProcess(2);
    }
    say("GUARD_TEST pattern verified and original restored\r\n");
    CloseHandle(h); VirtualFree(buf,0,MEM_RELEASE); ExitProcess(0);
}
