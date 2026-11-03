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
struct worker_ctx { DWORD index; unsigned char original[512]; int ok; };
static struct worker_ctx ctx[8];
static DWORD WINAPI worker(void *context) {
    struct worker_ctx *w=(struct worker_ctx *)context;
    HANDLE h=INVALID_HANDLE_VALUE; unsigned char *pattern=NULL,*check=NULL;
    LARGE_INTEGER off; DWORD n=0,i,j; int changed=0,ok=0;
    h=CreateFileW(L"\\\\.\\PhysicalDrive0",GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_NO_BUFFERING,NULL);
    pattern=VirtualAlloc(NULL,BYTES,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    check=VirtualAlloc(NULL,BYTES,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(h==INVALID_HANDLE_VALUE || !pattern || !check) goto done;
    off.QuadPart=((LONGLONG)(A04_SCRATCH_LBA+w->index))<<9;
    for(i=0;i<128u;++i) {
        for(j=0;j<512u;++j) pattern[j]=(unsigned char)((j*37u+i*73u+w->index*11u)^(j>>5));
        changed=1;
        if(!SetFilePointerEx(h,off,NULL,FILE_BEGIN) || !WriteFile(h,pattern,512,&n,NULL) || n!=512 ||
           !SetFilePointerEx(h,off,NULL,FILE_BEGIN) || !ReadFile(h,check,512,&n,NULL) || n!=512) goto restore;
        for(j=0;j<512u;++j) if(pattern[j]!=check[j]) goto restore;
    }
    ok=1;
restore:
    for(j=0;j<512u;++j) pattern[j]=w->original[j];
    if(changed && (!SetFilePointerEx(h,off,NULL,FILE_BEGIN) || !WriteFile(h,pattern,512,&n,NULL) || n!=512 ||
        !SetFilePointerEx(h,off,NULL,FILE_BEGIN) || !ReadFile(h,check,512,&n,NULL) || n!=512)) ok=0;
    if(changed) for(j=0;j<512u;++j) if(pattern[j]!=check[j]) ok=0;
done:
    if(pattern) VirtualFree(pattern,0,MEM_RELEASE);
    if(check) VirtualFree(check,0,MEM_RELEASE);
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    w->ok=ok; return ok?0:1;
}
void mainCRTStartup(void) {
    HANDLE h=INVALID_HANDLE_VALUE, backup=INVALID_HANDLE_VALUE, threads[8]={0};
    unsigned char *original=NULL,*check=NULL;
    GET_LENGTH_INFORMATION length; DWORD n=0,i,j,started=0; int ok=0;
    h=CreateFileW(L"\\\\.\\PhysicalDrive0",GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_NO_BUFFERING,NULL);
    if(h==INVALID_HANDLE_VALUE) goto done;
    if(!DeviceIoControl(h,IOCTL_DISK_GET_LENGTH_INFO,NULL,0,&length,sizeof(length),&n,NULL) ||
       n<sizeof(length) || length.Length.QuadPart<((LONGLONG)(A04_SCRATCH_LBA+16u)<<9)) goto done;
    original=VirtualAlloc(NULL,BYTES,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    check=VirtualAlloc(NULL,BYTES,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!original || !check || !io(h,A04_SCRATCH_LBA,original,0)) goto done;
    backup=CreateFileW(L"X:\\a04br3\\scratch-original.bin",GENERIC_READ,FILE_SHARE_READ,
                       NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(backup==INVALID_HANDLE_VALUE || GetFileSize(backup,NULL)!=BYTES ||
       !ReadFile(backup,check,BYTES,&n,NULL) || n!=BYTES || !same(original,check)) goto done;
    CloseHandle(backup); backup=INVALID_HANDLE_VALUE;
    say("MSDC_SCRATCH_PARALLEL_STARTED\r\n");
    for(i=0;i<8u;++i) {
        ctx[i].index=i;
        for(j=0;j<512u;++j) ctx[i].original[j]=original[i*512u+j];
        threads[i]=CreateThread(NULL,0,worker,&ctx[i],0,NULL);
        if(!threads[i]) break;
        ++started;
    }
    if(started && WaitForMultipleObjects(started,threads,TRUE,INFINITE)==WAIT_FAILED) goto done;
    if(started!=8u) goto done;
    for(i=0;i<8u;++i) if(!ctx[i].ok) goto done;
    if(!io(h,A04_SCRATCH_LBA,check,0) || !same(original,check)) goto done;
    if(!io(h,A04_SCRATCH_LBA+8u,check,0)) goto done;
    ok=1;
done:
    say(ok?"MSDC_SCRATCH_PARALLEL_PASSED\r\n":"MSDC_SCRATCH_PARALLEL_FAILED\r\n");
    for(i=0;i<started;++i) CloseHandle(threads[i]);
    if(original) VirtualFree(original,0,MEM_RELEASE);
    if(check) VirtualFree(check,0,MEM_RELEASE);
    if(backup!=INVALID_HANDLE_VALUE) CloseHandle(backup);
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    ExitProcess(ok?0:1);
}
