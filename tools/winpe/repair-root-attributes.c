/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental targeted root-attribute repair for the A04BR3 Windows volume.
 * Never run without an independently verified pre-repair volume backup. */
#include <windows.h>
#include <winioctl.h>

#ifndef A04_REPAIR_DRIVE
#define A04_REPAIR_DRIVE L'C'
#endif
#ifndef A04_REPAIR_DISK
#define A04_REPAIR_DISK 0
#endif
#ifndef A04_REPAIR_OFFSET
#define A04_REPAIR_OFFSET 469762048LL
#endif
#ifndef A04_REPAIR_LENGTH
#define A04_REPAIR_LENGTH 15147204608LL
#endif

static void say(const char *text)
{
    DWORD n = 0, written;
    while (text[n]) ++n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, n, &written, NULL);
}

void mainCRTStartup(void)
{
    WCHAR root[] = L"C:\\", volume[] = L"\\\\.\\C:", marker[] = L"C:\\Windows\\System32\\config\\SYSTEM";
    WCHAR label[64];
    VOLUME_DISK_EXTENTS extents;
    DWORD size = 0, flags, before, after;
    HANDLE h;
    root[0] = A04_REPAIR_DRIVE; volume[4] = A04_REPAIR_DRIVE; marker[0] = A04_REPAIR_DRIVE;
    if (!GetVolumeInformationW(root, label, 64, NULL, NULL, &flags, NULL, 0) ||
        lstrcmpiW(label, L"Windows") != 0 || GetFileAttributesW(marker) == INVALID_FILE_ATTRIBUTES) {
        say("ROOT_REPAIR volume label or Windows marker rejected\r\n"); ExitProcess(2);
    }
    h = CreateFileW(volume, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { say("ROOT_REPAIR volume open failed\r\n"); ExitProcess(2); }
    if (!DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0,
                         &extents, sizeof(extents), &size, NULL) ||
        size < sizeof(extents) || extents.NumberOfDiskExtents != 1 ||
        extents.Extents[0].DiskNumber != A04_REPAIR_DISK ||
        extents.Extents[0].StartingOffset.QuadPart != A04_REPAIR_OFFSET ||
        extents.Extents[0].ExtentLength.QuadPart != A04_REPAIR_LENGTH) {
        CloseHandle(h); say("ROOT_REPAIR disk extent rejected\r\n"); ExitProcess(2);
    }
    CloseHandle(h);
    before = GetFileAttributesW(root);
    if (before == INVALID_FILE_ATTRIBUTES || (before & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))) {
        say("ROOT_REPAIR unexpected existing attributes; no change\r\n"); ExitProcess(2);
    }
    flags = (before & ~(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_NORMAL)) |
            FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
    if (!SetFileAttributesW(root, flags)) { say("ROOT_REPAIR SetFileAttributes failed\r\n"); ExitProcess(1); }
    after = GetFileAttributesW(root);
    if (after == INVALID_FILE_ATTRIBUTES ||
        (after & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) !=
        (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) {
        say("ROOT_REPAIR attribute verification failed\r\n"); ExitProcess(1);
    }
    say("ROOT_REPAIR root attributes set; run read-only chkdsk and dismount\r\n");
    ExitProcess(0);
}
