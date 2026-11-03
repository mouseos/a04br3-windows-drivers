/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Prevent accidental console text selection from pausing an unattended
 * WinPE update while touch is being tested. Uses the inherited console. */
#include <windows.h>
void mainCRTStartup(void)
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0, check = 0;
    if (h == INVALID_HANDLE_VALUE || !GetConsoleMode(h, &mode)) ExitProcess(1);
    mode = (mode | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE;
    if (!SetConsoleMode(h, mode) || !GetConsoleMode(h, &check) ||
        (check & ENABLE_QUICK_EDIT_MODE)) ExitProcess(2);
    ExitProcess(0);
}
