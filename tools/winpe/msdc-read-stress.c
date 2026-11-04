/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * WinPE-only physical-disk read test. Never opens a device for writing. */
#include <windows.h>
#include <winioctl.h>
#include <bcrypt.h>
#define BLOCK_BYTES 4096u
#define BASE_LBA 0x000e1000u
#define START_LBA 0x000e0100u
#define END_LBA 0x000e0200u
static int read_block(HANDLE h, DWORD lba, unsigned char *buffer);
static void say(const char *s) {
    DWORD n=0, done=0; while (s[n]) ++n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&done,NULL);
}
static void number(DWORD value) {
    char digits[11]; unsigned n=0; DWORD written;
    do { digits[n++]=(char)('0'+value%10); value/=10; } while (value);
    while (n) { --n; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),digits+n,1,&written,NULL); }
}
/* Independently compare this digest with a recovery-side raw read; computing
 * it after the timed loop keeps crypto overhead out of the throughput result. */
static int reference_hash(HANDLE h, unsigned char *buffer) {
    BCRYPT_ALG_HANDLE alg=NULL; BCRYPT_HASH_HANDLE hash=NULL;
    unsigned char *object=NULL, digest[32]; DWORD object_size=0, returned=0, i;
    static const char hex[]="0123456789abcdef"; char text[65]; int ok=0;
    if (BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)<0) goto done;
    if (BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,(PUCHAR)&object_size,sizeof(object_size),&returned,0)<0 ||
        returned!=sizeof(object_size) || !object_size || object_size>65536u) goto done;
    object=(unsigned char *)VirtualAlloc(NULL,object_size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!object || BCryptCreateHash(alg,&hash,object,object_size,NULL,0,0)<0) goto done;
    for (i=0;i<4096u;++i) {
        if (!read_block(h,BASE_LBA+i*8u,buffer) || BCryptHashData(hash,buffer,BLOCK_BYTES,0)<0) goto done;
    }
    if (BCryptFinishHash(hash,digest,sizeof(digest),0)<0) goto done;
    for (i=0;i<32;++i) { text[i*2]=hex[digest[i]>>4]; text[i*2+1]=hex[digest[i]&15]; }
    text[64]=0; say("MSDC_REFERENCE_SHA256="); say(text); say("\r\n"); ok=1;
done:
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg,0);
    if (object) VirtualFree(object,0,MEM_RELEASE);
    return ok;
}
static int read_block(HANDLE h, DWORD lba, unsigned char *buffer) {
    LARGE_INTEGER offset; DWORD got=0;
    offset.QuadPart=((LONGLONG)lba)<<9;
    return SetFilePointerEx(h,offset,NULL,FILE_BEGIN) &&
           ReadFile(h,buffer,BLOCK_BYTES,&got,NULL) && got==BLOCK_BYTES;
}
void mainCRTStartup(void) {
    HANDLE h=INVALID_HANDLE_VALUE; unsigned char *a=NULL,*b=NULL;
    GET_LENGTH_INFORMATION length; DWORD got=0, start, i, j;
    h=CreateFileW(L"\\\\.\\PhysicalDrive0",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                  NULL,OPEN_EXISTING,FILE_FLAG_NO_BUFFERING,NULL);
    if (h==INVALID_HANDLE_VALUE) goto fail;
    if (!DeviceIoControl(h,IOCTL_DISK_GET_LENGTH_INFO,NULL,0,&length,sizeof(length),&got,NULL) ||
        got<sizeof(length) || length.Length.QuadPart < ((LONGLONG)(BASE_LBA+32768u)<<9)) goto fail;
    a=(unsigned char *)VirtualAlloc(NULL,BLOCK_BYTES,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    b=(unsigned char *)VirtualAlloc(NULL,BLOCK_BYTES,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!a || !b || !read_block(h,0,a)) goto fail;
    if (a[512]!='E' || a[513]!='F' || a[514]!='I' || a[515]!=' ' ||
        a[516]!='P' || a[517]!='A' || a[518]!='R' || a[519]!='T') goto fail;
    if (!read_block(h,START_LBA,a)) goto fail;
    say("MSDC_READ_STRESS_STARTED\r\n");
    start=GetTickCount();
    for (i=0; i<16384u && (DWORD)(GetTickCount()-start)<60000u; ++i) {
        DWORD lba=BASE_LBA+(i%4096u)*8u;
        if (!read_block(h,lba,a) || !read_block(h,lba,b)) goto fail;
        for (j=0; j<BLOCK_BYTES; ++j) if (a[j]!=b[j]) goto fail;
    }
    if (i<128u || !read_block(h,END_LBA,a)) goto fail;
    say("MSDC_READ_STRESS_PAIRS="); number(i);
    say(" ELAPSED_MS="); number((DWORD)(GetTickCount()-start)); say("\r\n");
    if (!reference_hash(h,a)) goto fail;
    say("MSDC_READ_STRESS_PASSED\r\n");
    VirtualFree(a,0,MEM_RELEASE); VirtualFree(b,0,MEM_RELEASE); CloseHandle(h); ExitProcess(0);
fail:
    say("MSDC_READ_STRESS_FAILED\r\n");
    if (a) VirtualFree(a,0,MEM_RELEASE); if (b) VirtualFree(b,0,MEM_RELEASE);
    if (h!=INVALID_HANDLE_VALUE) CloseHandle(h); ExitProcess(1);
}
