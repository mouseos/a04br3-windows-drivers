/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Copy a small generated BCD to a host-backed-up scratch area, then recover it
 * through Android recovery. No writes outside the validated Windows partition. */
#include <windows.h>
#include <winioctl.h>
#include "gpt_guard.h"
#define SCRATCH_OFFSET 2048u
#define SCRATCH_BLOCKS 128u
static void say(const char *s)
{ DWORD n=0,w; while(s[n]) ++n; (void)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,NULL); }
static int sector(HANDLE h, void *buf, uint32_t lba, int write)
{
    LARGE_INTEGER at; DWORD n=0; at.QuadPart=(LONGLONG)lba*512;
    if (!SetFilePointerEx(h,at,NULL,FILE_BEGIN)) return 0;
    return write ? WriteFile(h,buf,512,&n,NULL) && n==512 : ReadFile(h,buf,512,&n,NULL) && n==512;
}
static uint32_t crc32(const unsigned char *p, DWORD n)
{
    uint32_t c=0xffffffffu; DWORD i; unsigned j;
    for (i=0;i<n;++i) { c^=p[i]; for (j=0;j<8;++j) c=(c>>1)^(0xedb88320u & (0u-(c&1u))); }
    return c^0xffffffffu;
}
void mainCRTStartup(void)
{
    GET_LENGTH_INFORMATION length; DWORD n=0, size, blocks; uint32_t first,last,i;
    struct a04_gpt_guard guard; HANDLE disk=INVALID_HANDLE_VALUE, file=INVALID_HANDLE_VALUE;
    unsigned char *buf=(unsigned char *)VirtualAlloc(NULL,(SCRATCH_BLOCKS+1)*512,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!buf) { say("BCD_EXPORT allocation failed\r\n"); ExitProcess(1); }
    file=CreateFileW(L"X:\\a04-boot.bcd",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if (file==INVALID_HANDLE_VALUE || (size=GetFileSize(file,NULL))==INVALID_FILE_SIZE ||
        !size || size>(SCRATCH_BLOCKS-1)*512u || !ReadFile(file,buf+512,size,&n,NULL) || n!=size) {
        say("BCD_EXPORT BCD input rejected\r\n"); ExitProcess(1);
    }
    CloseHandle(file); blocks=(size+511)/512;
    disk=CreateFileW(L"\\\\.\\PhysicalDrive0",GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_NO_BUFFERING,NULL);
    if (disk==INVALID_HANDLE_VALUE || !DeviceIoControl(disk,IOCTL_DISK_GET_LENGTH_INFO,NULL,0,&length,sizeof(length),&n,NULL) ||
        length.Length.QuadPart!=(LONGLONG)30535680*512 || !sector(disk,buf,1,0) ||
        !a04_gpt_guard_begin(&guard,buf,30535680)) { say("BCD_EXPORT device or GPT rejected\r\n"); ExitProcess(1); }
    for (i=2;i<34;++i) if (!sector(disk,buf,i,0) || !a04_gpt_guard_feed(&guard,buf)) {
        say("BCD_EXPORT GPT entries rejected\r\n"); ExitProcess(1);
    }
    if (!a04_gpt_guard_finish(&guard,&first,&last) || first!=917504 ||
        !a04_gpt_guard_range(first,last,(uint64_t)first+SCRATCH_OFFSET,SCRATCH_BLOCKS)) {
        say("BCD_EXPORT scratch range rejected\r\n"); ExitProcess(1);
    }
    for (i=0;i<SCRATCH_BLOCKS;++i) {
        DWORD j; if (!sector(disk,buf,first+SCRATCH_OFFSET+i,0)) { say("BCD_EXPORT scratch read failed\r\n"); ExitProcess(1); }
        for (j=0;j<512;++j) if (buf[j]) { say("BCD_EXPORT scratch not zero\r\n"); ExitProcess(1); }
    }
    say("BCD_EXPORT starting scratch writes\r\n");
    for (i=0;i<blocks;++i) if (!sector(disk,buf+512+i*512,first+SCRATCH_OFFSET+1+i,1)) {
        say("BCD_EXPORT write failed; restore scratch in recovery\r\n"); ExitProcess(2);
    }
    for (i=0;i<512;++i) buf[i]=0;
    buf[0]='A';buf[1]='0';buf[2]='4';buf[3]='B';buf[4]='C';buf[5]='D';buf[6]='1';
    { uint32_t c=crc32(buf+512,size); for (i=0;i<4;++i) {buf[8+i]=(unsigned char)(size>>(8*i));buf[12+i]=(unsigned char)(c>>(8*i));} }
    if (!sector(disk,buf,first+SCRATCH_OFFSET,1) || !FlushFileBuffers(disk)) {
        say("BCD_EXPORT header write failed; restore scratch in recovery\r\n"); ExitProcess(2);
    }
    say("BCD_EXPORT complete; restore scratch after host retrieval\r\n");
    CloseHandle(disk);VirtualFree(buf,0,MEM_RELEASE);ExitProcess(0);
}
