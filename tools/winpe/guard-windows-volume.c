/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only identity guard for a narrowly scoped WinPE diagnostic. */
#include <windows.h>
#include <winioctl.h>
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
static WCHAR root[] = L"C:\\";
static WCHAR volume[] = L"\\\\.\\C:";
static WCHAR marker[] = L"C:\\Windows\\System32\\config\\SYSTEM";
static void say(const char *s)
{
    DWORD n=0, done; while (s[n]) ++n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s, n, &done, NULL);
}
void mainCRTStartup(void)
{
    WCHAR label[64], fs[32]; VOLUME_DISK_EXTENTS extents;
    DWORD size=0, flags, attrs; HANDLE h;
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
