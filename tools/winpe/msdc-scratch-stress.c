/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * WinPE-only bounded diagnostic. The host must independently save and restore
 * this GPT-unallocated range through recovery, even after an apparent pass. */
#include <windows.h>
#include <winioctl.h>
#ifndef A04_SCRATCH_LBA
#error Select the same eight-sector LBA as the diagnostic miniport
#endif
#define BYTES 4096u
static void say(const char *s) { DWORD n=0,w; while(s[n]) ++n; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,NULL); }
static int io(HANDLE h, DWORD lba, void *buffer, int write) {
    LARGE_INTEGER o; DWORD n=0; o.QuadPart=((LONGLONG)lba)<<9;
    if (!SetFilePointerEx(h,o,NULL,FILE_BEGIN)) return 0;
    return (write ? WriteFile(h,buffer,BYTES,&n,NULL) : ReadFile(h,buffer,BYTES,&n,NULL)) && n==BYTES;
}
static int same(const unsigned char *a,const unsigned char *b) {
    DWORD i; for(i=0;i<BYTES;++i) if(a[i]!=b[i]) return 0; return 1;
}
void mainCRTStartup(void) {
    HANDLE h=INVALID_HANDLE_VALUE, backup=INVALID_HANDLE_VALUE; unsigned char *original=NULL,*pattern=NULL,*readback=NULL;
    GET_LENGTH_INFORMATION length; DWORD got=0,i,j; int modified=0,ok=0,restored=0;
    h=CreateFileW(L"\\\\.\\PhysicalDrive0",GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_NO_BUFFERING,NULL);
    if(h==INVALID_HANDLE_VALUE) goto done;
    if(!DeviceIoControl(h,IOCTL_DISK_GET_LENGTH_INFO,NULL,0,&length,sizeof(length),&got,NULL) ||
       got<sizeof(length) || length.Length.QuadPart<((LONGLONG)(A04_SCRATCH_LBA+16u)<<9)) goto done;
    original=VirtualAlloc(NULL,BYTES,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    pattern=VirtualAlloc(NULL,BYTES,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    readback=VirtualAlloc(NULL,BYTES,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!original || !pattern || !readback || !io(h,A04_SCRATCH_LBA,original,0)) goto done;
    /* Refuse to write unless this is the exact sector image captured by the
     * host immediately before building this disposable diagnostic WinPE. */
    backup=CreateFileW(L"X:\\a04br3\\scratch-original.bin",GENERIC_READ,FILE_SHARE_READ,
                       NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(backup==INVALID_HANDLE_VALUE || GetFileSize(backup,NULL)!=BYTES ||
       !ReadFile(backup,readback,BYTES,&got,NULL) || got!=BYTES || !same(original,readback)) goto done;
    CloseHandle(backup); backup=INVALID_HANDLE_VALUE;
    say("MSDC_SCRATCH_STRESS_STARTED\r\n");
    for(i=0;i<128u;++i) {
        for(j=0;j<BYTES;++j) pattern[j]=(unsigned char)((j*37u+i*73u)^(j>>5));
        modified=1;
        if(!io(h,A04_SCRATCH_LBA,pattern,1) || !io(h,A04_SCRATCH_LBA,readback,0) ||
           !same(pattern,readback)) goto restore;
    }
    ok=1;
restore:
    if(modified && io(h,A04_SCRATCH_LBA,original,1) && io(h,A04_SCRATCH_LBA,readback,0) &&
       same(original,readback)) restored=1;
    if(!restored) ok=0;
    if(ok && !io(h,A04_SCRATCH_LBA+8u,readback,0)) ok=0;
done:
    say(ok ? "MSDC_SCRATCH_STRESS_PASSED\r\n" : "MSDC_SCRATCH_STRESS_FAILED\r\n");
    if(original) VirtualFree(original,0,MEM_RELEASE);
    if(pattern) VirtualFree(pattern,0,MEM_RELEASE);
    if(readback) VirtualFree(readback,0,MEM_RELEASE);
    if(backup!=INVALID_HANDLE_VALUE) CloseHandle(backup);
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    ExitProcess(ok?0:1);
}
