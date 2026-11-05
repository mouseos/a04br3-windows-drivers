/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Copy an explicitly named registry file, including hidden/system files.
 * Refuse to overwrite an existing destination. */
#include <windows.h>
static WCHAR command[4096];
static WCHAR *next_arg(WCHAR **cursor)
{
    WCHAR *p = *cursor, *start;
    while (*p == L' ' || *p == L'\t') ++p;
    if (!*p) { *cursor = p; return NULL; }
    if (*p == L'"') {
        start = ++p;
        while (*p && *p != L'"') ++p;
        if (!*p) return NULL;
        *p++ = 0;
        if (*p && *p != L' ' && *p != L'\t') return NULL;
    } else {
        start = p;
        while (*p && *p != L' ' && *p != L'\t') ++p;
        if (*p) *p++ = 0;
    }
    *cursor = p;
    return start;
}
void mainCRTStartup(void)
{
    const WCHAR *raw = GetCommandLineW();
    WCHAR *cursor, *source, *destination;
    DWORD i;
    for (i = 0; raw[i] && i < 4095; ++i) command[i] = raw[i];
    if (raw[i]) ExitProcess(1);
    command[i] = 0; cursor = command;
    if (!next_arg(&cursor) || !(source = next_arg(&cursor)) ||
        !(destination = next_arg(&cursor)) || next_arg(&cursor)) ExitProcess(1);
    if (!CopyFileW(source, destination, TRUE)) ExitProcess(2);
    ExitProcess(0);
}
