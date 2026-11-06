/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Flush only after the caller has identified the offline volume and unloaded
 * its hives. This is not a volume identity guard and never dismounts a volume. */
#include <windows.h>

static void say(const char *s)
{
    DWORD n=0, written=0;
    while (s[n]) ++n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&written,NULL);
}

static int absent(const WCHAR *name)
{
    HKEY key=NULL;
    LONG status=RegOpenKeyExW(HKEY_LOCAL_MACHINE,name,0,KEY_READ,&key);
    if (status==ERROR_SUCCESS) {
        RegCloseKey(key);
        return 0;
    }
    /* Access denied and all other unknown states are failures, not absence. */
    return status==ERROR_FILE_NOT_FOUND;
}

void mainCRTStartup(void)
{
    HANDLE volume;
    BOOL flushed, closed;
    if (!absent(L"A04TOPRGUSYS") || !absent(L"A04TOPRGUSW")) {
        say("TOPRGU_FLUSH_HIVE_LOADED_OR_UNKNOWN\r\n");
        ExitProcess(2);
    }
    volume=CreateFileW(L"\\\\.\\C:",GENERIC_WRITE,
                       FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if (volume==INVALID_HANDLE_VALUE) {
        say("TOPRGU_FLUSH_VOLUME_OPEN_FAILED\r\n");
        ExitProcess(3);
    }
    flushed=FlushFileBuffers(volume);
    closed=CloseHandle(volume);
    if (!flushed || !closed) {
        say("TOPRGU_FLUSH_VOLUME_FAILED\r\n");
        ExitProcess(4);
    }
    say("TOPRGU_VOLUME_FLUSHED_NOT_DISMOUNTED\r\n");
    ExitProcess(0);
}
