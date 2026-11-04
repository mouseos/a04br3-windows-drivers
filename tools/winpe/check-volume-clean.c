/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only dirty-bit check; the separate volume identity guard is mandatory.
 * A clean dirty bit does not prove an undamaged filesystem. */
#include <windows.h>
#include <winioctl.h>
void mainCRTStartup(void)
{
    HANDLE h; DWORD flags=0, returned=0; BOOL ok;
    h=CreateFileW(L"\\\\.\\C:",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                  NULL,OPEN_EXISTING,0,NULL);
    if (h==INVALID_HANDLE_VALUE) ExitProcess(2);
    ok=DeviceIoControl(h,FSCTL_IS_VOLUME_DIRTY,NULL,0,&flags,sizeof(flags),&returned,NULL);
    CloseHandle(h);
    if (!ok || returned!=sizeof(flags) || flags!=0) ExitProcess(2);
    ExitProcess(0);
}
