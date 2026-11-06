/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental release primitive, not an updater. Caller must unload every
 * hive, close its handles, and prevent other processes from reopening C:.
 * A successful dismount is not a filesystem integrity check. */
#include <windows.h>
#include <winioctl.h>

static void say(const char *s) {
    DWORD n=0, written; while (s[n]) ++n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&written,NULL);
}
static void failure(const char *step, DWORD error) {
    static const char digits[]="0123456789ABCDEF";
    char out[11]; unsigned i;
    out[0]='0'; out[1]='x';
    for (i=0; i<8; ++i) out[2+i]=digits[(error>>(28-4*i))&15];
    out[10]=0; say(step); say(" error="); say(out); say("\r\n");
}
static int hive_absent(const WCHAR *name) {
    HKEY key; LONG rc=RegOpenKeyExW(HKEY_LOCAL_MACHINE,name,0,KEY_READ,&key);
    if (rc==ERROR_SUCCESS) { RegCloseKey(key); return 0; }
    return rc==ERROR_FILE_NOT_FOUND;
}
void mainCRTStartup(void) {
    HANDLE h; DWORD n=0, err; VOLUME_DISK_EXTENTS ex; WCHAR label[64], fs[32];
    if (!hive_absent(L"A04TOPRGUSYS") || !hive_absent(L"A04TOPRGUSW") ||
        !hive_absent(L"A04TXSYS") || !hive_absent(L"A04TXSW") ||
        !hive_absent(L"A04AUDITSYS") || !hive_absent(L"A04TEMP")) {
        say("A04_RELEASE_HIVE_LOADED_OR_UNKNOWN\r\n"); ExitProcess(2);
    }
    if (!GetVolumeInformationW(L"C:\\",label,64,NULL,NULL,NULL,fs,32) ||
        lstrcmpiW(label,L"Windows") || lstrcmpiW(fs,L"NTFS") ||
        GetFileAttributesW(L"C:\\Windows\\System32\\config\\SYSTEM")==INVALID_FILE_ATTRIBUTES) {
        say("A04_RELEASE_IDENTITY_REJECTED\r\n"); ExitProcess(3);
    }
    h=CreateFileW(L"\\\\.\\C:",GENERIC_READ|GENERIC_WRITE,
                  FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if (h==INVALID_HANDLE_VALUE) { failure("A04_RELEASE_OPEN_FAILED",GetLastError()); ExitProcess(4); }
    if (!DeviceIoControl(h,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,NULL,0,&ex,sizeof(ex),&n,NULL) ||
        n<sizeof(ex) || ex.NumberOfDiskExtents!=1 || ex.Extents[0].DiskNumber!=0 ||
        ex.Extents[0].StartingOffset.QuadPart!=469762048LL ||
        ex.Extents[0].ExtentLength.QuadPart!=15147204608LL) {
        say("A04_RELEASE_EXTENT_REJECTED\r\n"); CloseHandle(h); ExitProcess(5);
    }
    if (!DeviceIoControl(h,FSCTL_LOCK_VOLUME,NULL,0,NULL,0,&n,NULL)) {
        err=GetLastError(); CloseHandle(h); failure("A04_RELEASE_LOCK_FAILED",err); ExitProcess(6);
    }
    say("A04_RELEASE_LOCKED\r\n");
    if (!FlushFileBuffers(h)) {
        err=GetLastError(); CloseHandle(h); failure("A04_RELEASE_FLUSH_FAILED",err); ExitProcess(7);
    }
    say("A04_RELEASE_FLUSHED\r\n");
    if (!DeviceIoControl(h,FSCTL_DISMOUNT_VOLUME,NULL,0,NULL,0,&n,NULL)) {
        err=GetLastError(); CloseHandle(h); failure("A04_RELEASE_DISMOUNT_FAILED",err); ExitProcess(8);
    }
    say("A04_RELEASE_DISMOUNTED\r\n");
    if (!CloseHandle(h)) { failure("A04_RELEASE_CLOSE_FAILED",GetLastError()); ExitProcess(9); }
    if (!DeleteVolumeMountPointW(L"C:\\")) { failure("A04_RELEASE_UNMOUNT_FAILED",GetLastError()); ExitProcess(10); }
    n=GetLogicalDrives();
    if (!n || (n & (1u << (L'C'-L'A')))) { say("A04_RELEASE_DRIVE_STILL_PRESENT_OR_UNKNOWN\r\n"); ExitProcess(11); }
    say("A04_RELEASE_UNMOUNTED_NO_INTEGRITY_CLAIM\r\n"); ExitProcess(0);
}
