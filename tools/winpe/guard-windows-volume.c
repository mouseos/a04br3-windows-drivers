/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only identity guard for a narrowly scoped WinPE diagnostic. */
#include <windows.h>
#include <winioctl.h>
#include "check-chkdsk.h"
#ifndef A04_GUARD_DRIVE
#define A04_GUARD_DRIVE L'C'
#endif
#ifndef A04_GUARD_DISK
#define A04_GUARD_DISK 0
#endif
#ifndef A04_GUARD_OFFSET
#define A04_GUARD_OFFSET 469762048LL
#endif
#ifndef A04_GUARD_LENGTH
#define A04_GUARD_LENGTH 15147204608LL
#endif
#ifndef A04_CHECK_DRIVE
#define A04_CHECK_DRIVE L'X'
#endif
static WCHAR root[] = L"C:\\";
static WCHAR volume[] = L"\\\\.\\C:";
static WCHAR marker[] = L"C:\\Windows\\System32\\config\\SYSTEM";
static void say(const char *s)
{
    DWORD n=0, done; while (s[n]) ++n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s, n, &done, NULL);
}
static int has_option(const WCHAR *s, const WCHAR *needle)
{
    unsigned i, j;
    for (i=0; s[i]; ++i) {
        for (j=0; needle[j] && s[i+j] && s[i+j]==needle[j]; ++j) {}
        if (!needle[j]) return 1;
    }
    return 0;
}
static int check_chkdsk(void)
{
    HANDLE h; char *data; DWORD size, got=0; int ok;
    WCHAR path[]=L"X:\\a04br3\\check-before.txt";
    path[0]=A04_CHECK_DRIVE;
    h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if (h==INVALID_HANDLE_VALUE) return 0;
    size=GetFileSize(h,NULL);
    if (size==INVALID_FILE_SIZE || !size || size>1048576) { CloseHandle(h); return 0; }
    data=(char *)HeapAlloc(GetProcessHeap(),0,size);
    if (!data) { CloseHandle(h); return 0; }
    ok=ReadFile(h,data,size,&got,NULL) && got==size;
    CloseHandle(h);
    if (ok) ok=a04_chkdsk_expected(data,size);
    HeapFree(GetProcessHeap(),0,data);
    return ok;
}
void mainCRTStartup(void)
{
    WCHAR label[64], fs[32]; VOLUME_DISK_EXTENTS extents;
    DWORD size=0, flags, attrs; HANDLE h;
    if (has_option(GetCommandLineW(),L"--check-chkdsk")) {
        if (check_chkdsk()) { say("CHKDSK_EXPECTED_INDEX_ONLY\r\n"); ExitProcess(0); }
        say("CHKDSK_OUTPUT_REJECTED\r\n"); ExitProcess(2);
    }
    root[0]=A04_GUARD_DRIVE; volume[4]=A04_GUARD_DRIVE; marker[0]=A04_GUARD_DRIVE;
    if (!GetVolumeInformationW(root,label,64,NULL,NULL,&flags,fs,32) ||
        lstrcmpiW(label,L"Windows") || lstrcmpiW(fs,L"NTFS") ||
        GetFileAttributesW(marker)==INVALID_FILE_ATTRIBUTES) {
        say("VOLUME_GUARD label, filesystem or marker rejected\r\n"); ExitProcess(2);
    }
    attrs=GetFileAttributesW(root);
    if (attrs==INVALID_FILE_ATTRIBUTES ||
        (attrs & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)) !=
        (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)) {
        say("VOLUME_GUARD root attributes rejected\r\n"); ExitProcess(2);
    }
    h=CreateFileW(volume,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if (h==INVALID_HANDLE_VALUE) { say("VOLUME_GUARD open failed\r\n"); ExitProcess(2); }
    if (!DeviceIoControl(h,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,NULL,0,&extents,sizeof(extents),&size,NULL) ||
        size<sizeof(extents) || extents.NumberOfDiskExtents!=1 ||
        extents.Extents[0].DiskNumber!=A04_GUARD_DISK ||
        extents.Extents[0].StartingOffset.QuadPart!=A04_GUARD_OFFSET ||
        extents.Extents[0].ExtentLength.QuadPart!=A04_GUARD_LENGTH) {
        CloseHandle(h); say("VOLUME_GUARD disk extent rejected\r\n"); ExitProcess(2);
    }
    CloseHandle(h); say("VOLUME_GUARD_OK\r\n"); ExitProcess(0);
}
