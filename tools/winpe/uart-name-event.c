/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <windows.h>
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"

void mainCRTStartup(void)
{
    const WCHAR *cmd = GetCommandLineW();
    const char *message = NULL;
    DWORD len = 0, sent, start = GetTickCount();
    HANDLE port;
    while (*cmd) {
        if (*cmd == L' ' && cmd[1] == L'S' && cmd[2] == L'T' && cmd[3] == L'A' && cmd[4] == L'R' && cmd[5] == L'T' && !cmd[6]) message = "A04NAMES START\n";
        if (*cmd == L' ' && cmd[1] == L'S' && cmd[2] == L'T' && cmd[3] == L'A' && cmd[4] == L'G' && cmd[5] == L'E' && cmd[6] == L'D' && !cmd[7]) message = "A04NAMES STAGED\n";
        if (*cmd == L' ' && cmd[1] == L'F' && cmd[2] == L'A' && cmd[3] == L'I' && cmd[4] == L'L' && cmd[5] == L'E' && cmd[6] == L'D' && !cmd[7]) message = "A04NAMES FAILED\n";
        if (*cmd == L' ' && cmd[1] == L'U' && cmd[2] == L'N' && cmd[3] == L'S' && cmd[4] == L'A' && cmd[5] == L'F' && cmd[6] == L'E' && !cmd[7]) message = "A04NAMES UNSAFE\n";
        if (*cmd == L' ' && cmd[1] == L'P' && cmd[2] == L'A' && cmd[3] == L'R' && cmd[4] == L'T' && cmd[5] == L'I' && cmd[6] == L'A' && cmd[7] == L'L' && !cmd[8]) message = "A04NAMES PARTIAL\n";
        ++cmd;
    }
    if (!message) ExitProcess(2);
    while (message[len]) ++len;
    port = CreateFileW(L"\\\\.\\A04BR3ScreenUart", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (port == INVALID_HANDLE_VALUE) ExitProcess(3);
    while (!DeviceIoControl(port, A04_UART_SCREEN_SEND, (void *)message, len, NULL, 0, &sent, NULL)) {
        if (GetLastError() != ERROR_BUSY || GetTickCount() - start > 10000u) { CloseHandle(port); ExitProcess(4); }
        Sleep(10);
    }
    Sleep(1000);
    CloseHandle(port);
    ExitProcess(0);
}
