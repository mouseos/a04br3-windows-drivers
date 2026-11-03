/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Compare two files without a C runtime or external command dependencies. */
#include <windows.h>

static WCHAR command[4096];
static BYTE left[4096], right[4096];

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
    WCHAR *p, *a, *b;
    DWORD i, n, na, nb;
    HANDLE x = INVALID_HANDLE_VALUE, y = INVALID_HANDLE_VALUE;
    int equal = 0;
    const WCHAR *raw = GetCommandLineW();
    for (n = 0; raw[n] && n < 4095; ++n) command[n] = raw[n];
    if (raw[n]) ExitProcess(1);
    command[n] = 0; p = command;
    if (!next_arg(&p) || !(a = next_arg(&p)) || !(b = next_arg(&p)) || next_arg(&p))
        ExitProcess(1);
    x = CreateFileW(a, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    y = CreateFileW(b, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (x == INVALID_HANDLE_VALUE || y == INVALID_HANDLE_VALUE) goto done;
    for (;;) {
        if (!ReadFile(x, left, sizeof(left), &na, NULL) ||
            !ReadFile(y, right, sizeof(right), &nb, NULL) || na != nb) goto done;
        for (i = 0; i < na; ++i) if (left[i] != right[i]) goto done;
        if (!na) { equal = 1; break; }
    }
done:
    if (x != INVALID_HANDLE_VALUE) CloseHandle(x);
    if (y != INVALID_HANDLE_VALUE) CloseHandle(y);
    ExitProcess(equal ? 0 : 1);
}
