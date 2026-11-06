/* Host fault injection of the Windows API call sequence; not an NTFS test. */
#include <stdio.h>
#include <setjmp.h>
#include <wchar.h>
#include <stdlib.h>
#include "windows.h"
#include "winioctl.h"
static jmp_buf done;
static unsigned exit_code, scenario, locked, flushed, dismounted, closed;
HANDLE GetStdHandle(DWORD n) { (void)n; return (HANDLE)1; }
BOOL WriteFile(HANDLE h,const void *p,DWORD n,DWORD *w,void *o) { (void)h;(void)p;(void)o;*w=n;return 1; }
LONG RegOpenKeyExW(HKEY h,const WCHAR *p,DWORD a,DWORD b,HKEY *k) { (void)h;(void)p;(void)a;(void)b;(void)k;return scenario==1 ? 5 : ERROR_FILE_NOT_FOUND; }
LONG RegCloseKey(HKEY h) { (void)h;return 0; }
BOOL GetVolumeInformationW(const WCHAR *p,WCHAR *l,DWORD a,DWORD *b,DWORD *c,DWORD *d,WCHAR *f,DWORD e) { (void)p;(void)a;(void)b;(void)c;(void)d;(void)e;wcscpy(l,L"Windows");wcscpy(f,L"NTFS");return scenario!=2; }
int lstrcmpiW(const WCHAR *a,const WCHAR *b) { return wcscmp(a,b); }
DWORD GetFileAttributesW(const WCHAR *p) { (void)p;return 0; }
HANDLE CreateFileW(const WCHAR *p,DWORD a,DWORD b,void *c,DWORD d,DWORD e,HANDLE f) { (void)p;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return scenario==3?INVALID_HANDLE_VALUE:(HANDLE)2; }
BOOL DeviceIoControl(HANDLE h,DWORD code,void *in,DWORD ins,void *out,DWORD outs,DWORD *n,void *over) {
    (void)h;(void)in;(void)ins;(void)outs;(void)over;
    if (code==IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS) {
        VOLUME_DISK_EXTENTS *v=out; *n=sizeof(*v);v->NumberOfDiskExtents=1;v->Extents[0].DiskNumber=0;
        v->Extents[0].StartingOffset.QuadPart=469762048LL;v->Extents[0].ExtentLength.QuadPart=scenario==4?1:15147204608LL;return 1;
    }
    if (code==FSCTL_LOCK_VOLUME) { ++locked;return scenario!=5; }
    if (code==FSCTL_DISMOUNT_VOLUME) { ++dismounted;return scenario!=7; }
    abort();
}
BOOL FlushFileBuffers(HANDLE h) { (void)h;++flushed;return scenario!=6; }
BOOL CloseHandle(HANDLE h) { (void)h;++closed;return scenario!=8; }
DWORD GetLastError(void) { return 123; }
void ExitProcess(DWORD code) { exit_code=code;longjmp(done,1); }
#include "../tools/winpe/release-offline-volume.c"
int main(void) {
    static const unsigned expected[]={0,2,3,4,5,6,7,8,9};
    for (scenario=0;scenario<9;++scenario) {
        locked=flushed=dismounted=closed=0;
        if (!setjmp(done)) mainCRTStartup();
        if (exit_code!=expected[scenario] || (scenario>=1 && scenario<=5 && dismounted) ||
            (scenario==6 && dismounted) || (scenario==0 && (locked!=1||flushed!=1||dismounted!=1||closed!=1))) {
            fprintf(stderr,"scenario %u failed: exit=%u lock=%u flush=%u dismount=%u close=%u\n",scenario,exit_code,locked,flushed,dismounted,closed);return 1;
        }
    }
    puts("offline release sequencing and failure injection: PASS");return 0;
}
