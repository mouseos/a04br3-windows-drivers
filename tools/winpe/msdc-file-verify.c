/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Limited filesystem-only write/read verification. No raw device path or LBA.
 * CREATE_NEW + a newly created directory prevent overwriting existing files. */
#define UNICODE
#define _UNICODE
#include <windows.h>

#define FILE_BYTES (64u * 1024u)
#define CHUNK_BYTES 4096u
static const WCHAR test_directory[] = L"C:\\Windows\\Temp\\MT8167MsdcFileVerify";
static const WCHAR test_file[] = L"C:\\Windows\\Temp\\MT8167MsdcFileVerify\\verify.bin";
static HANDLE output;
static BOOL output_ok = TRUE;
void *memset(void *dst, int value, size_t n)
{ unsigned char *p = dst; while (n--) *p++ = (unsigned char)value; return dst; }

static void say(const char *text)
{
    DWORD n = 0, done;
    while (text[n]) ++n;
    if (!WriteFile(output, text, n, &done, NULL) || done != n) output_ok = FALSE;
}
static void number(const char *key, DWORD value)
{
    const char hex[] = "0123456789ABCDEF";
    char text[96]; DWORD n = 0, i;
    while (*key && n < 70) text[n++] = *key++;
    text[n++] = '='; text[n++] = '0'; text[n++] = 'x';
    for (i = 0; i < 8; ++i) text[n++] = hex[(value >> (28 - 4 * i)) & 15];
    text[n++] = '\r'; text[n++] = '\n'; text[n] = 0; say(text);
}
static BYTE pattern(DWORD offset, DWORD pass)
{
    DWORD x = (offset >> 2) ^ (pass ? 0xa5c39e71u : 0x6d2b79f5u);
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
    return (BYTE)(x >> ((offset & 3u) * 8u));
}
static DWORD safe_directory(const WCHAR *path)
{
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) return GetLastError();
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT) ? NO_ERROR : ERROR_BAD_PATHNAME;
}
static DWORD preflight(void)
{
    WCHAR label[64], fs[32]; DWORD sector, clusters, free_clusters, total_clusters, flags;
    ULARGE_INTEGER available; DWORD error;
    /* Fixed normal filesystem only. No caller-provided path, volume GUID or LBA. */
    if (GetDriveTypeW(L"C:\\") != DRIVE_FIXED) return ERROR_BAD_DEVICE;
    if (!GetVolumeInformationW(L"C:\\", label, 64, NULL, NULL, &flags, fs, 32)) return GetLastError();
    if (lstrcmpiW(label, L"Windows") || lstrcmpiW(fs, L"NTFS") || (flags & FILE_READ_ONLY_VOLUME)) return ERROR_WRONG_DISK;
    if (!GetDiskFreeSpaceW(L"C:\\", &clusters, &sector, &free_clusters, &total_clusters)) return GetLastError();
    if (sector != 512) return ERROR_NOT_SUPPORTED;
    if (!GetDiskFreeSpaceExW(L"C:\\", &available, NULL, NULL)) return GetLastError();
    if (available.QuadPart < 16u * 1024u * 1024u) return ERROR_DISK_FULL;
    error = safe_directory(L"C:\\Windows"); if (error) return error;
    return safe_directory(L"C:\\Windows\\Temp");
}
static DWORD verify(void)
{
    HANDLE file = INVALID_HANDLE_VALUE, directory = INVALID_HANDLE_VALUE;
    BYTE *buffer = NULL; DWORD error, cleanup_error = NO_ERROR, pass, pos, n, done, i, start, requests;
    LARGE_INTEGER zero; BOOL created_directory = FALSE;
    error = preflight(); number("PREFLIGHT_ERROR", error); if (error) goto finish;
    if (!output_ok) { error = ERROR_WRITE_FAULT; goto finish; }
    /* Existing directories, including a previous interrupted run, are rejected. */
    if (!CreateDirectoryW(test_directory, NULL)) { error = GetLastError(); goto finish; }
    created_directory = TRUE;
    directory = CreateFileW(test_directory, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (directory == INVALID_HANDLE_VALUE) { error = GetLastError(); goto finish; }
    {
        BY_HANDLE_FILE_INFORMATION info;
        if (!GetFileInformationByHandle(directory, &info)) { error = GetLastError(); goto finish; }
        if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { error = ERROR_BAD_PATHNAME; goto finish; }
    }
    buffer = VirtualAlloc(NULL, CHUNK_BYTES, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buffer) { error = GetLastError(); goto finish; }
    /* All reads/writes are sector-aligned and bypass the Windows file cache.
     * DELETE_ON_CLOSE removes only this CREATE_NEW file, including on failure. */
    file = CreateFileW(test_file, GENERIC_READ | GENERIC_WRITE | DELETE, 0, NULL, CREATE_NEW,
                       FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH |
                       FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) { error = GetLastError(); goto finish; }
    number("FILE_BYTES", FILE_BYTES); number("PATTERN_PASSES", 2);
    zero.QuadPart = 0;
    for (pass = 0; pass < 2; ++pass) {
        number("PASS", pass);
        if (!SetFilePointerEx(file, zero, NULL, FILE_BEGIN)) { error = GetLastError(); goto finish; }
        start = GetTickCount(); requests = 0;
        for (pos = 0; pos < FILE_BYTES; pos += n) {
            /* First pass targets 4KiB I/O; second exercises 1/2/8-sector tails.
             * Filesystem request size does not itself prove the hardware CMD25. */
            n = !pass ? CHUNK_BYTES : (requests % 3 == 0 ? 512u : requests % 3 == 1 ? 1024u : CHUNK_BYTES);
            if (n > FILE_BYTES - pos) n = FILE_BYTES - pos;
            for (i = 0; i < n; ++i) buffer[i] = pattern(pos + i, pass);
            if (!WriteFile(file, buffer, n, &done, NULL)) { error = GetLastError(); goto finish; }
            if (done != n) { error = ERROR_WRITE_FAULT; goto finish; }
            ++requests;
        }
        if (!FlushFileBuffers(file)) { error = GetLastError(); goto finish; }
        number("WRITE_REQUESTS", requests); number("WRITE_FLUSH_MS", GetTickCount() - start);
        if (!SetFilePointerEx(file, zero, NULL, FILE_BEGIN)) { error = GetLastError(); goto finish; }
        start = GetTickCount(); requests = 0;
        for (pos = 0; pos < FILE_BYTES; pos += CHUNK_BYTES) {
            if (!ReadFile(file, buffer, CHUNK_BYTES, &done, NULL)) { error = GetLastError(); goto finish; }
            if (done != CHUNK_BYTES) { error = ERROR_READ_FAULT; goto finish; }
            ++requests;
            for (i = 0; i < CHUNK_BYTES; ++i) if (buffer[i] != pattern(pos + i, pass)) {
                number("MISMATCH_OFFSET", pos + i); number("EXPECTED_BYTE", pattern(pos + i, pass));
                number("ACTUAL_BYTE", buffer[i]); error = ERROR_CRC; goto finish;
            }
        }
        number("READ_REQUESTS", requests); number("READ_COMPARE_MS", GetTickCount() - start);
        number("VERIFIED_BYTES", FILE_BYTES);
    }
    error = NO_ERROR;
finish:
    if (file != INVALID_HANDLE_VALUE && !CloseHandle(file)) cleanup_error = GetLastError();
    if (buffer) VirtualFree(buffer, 0, MEM_RELEASE);
    if (directory != INVALID_HANDLE_VALUE) CloseHandle(directory);
    if (created_directory && !RemoveDirectoryW(test_directory) && !cleanup_error) cleanup_error = GetLastError();
    number("VERIFY_ERROR", error); number("CLEANUP_ERROR", cleanup_error);
    if (!error && cleanup_error) error = cleanup_error;
    if (!output_ok && !error) error = ERROR_WRITE_FAULT;
    say(error ? "RESULT=FAIL\r\n" : "RESULT=PASS\r\n");
    return !output_ok && !error ? ERROR_WRITE_FAULT : error;
}
void mainCRTStartup(void)
{
    const WCHAR *p = GetCommandLineW(); DWORD error;
    output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (*p == L'"') { ++p; while (*p && *p != L'"') ++p; if (*p) ++p; }
    else while (*p && *p != L' ' && *p != L'\t') ++p;
    while (*p == L' ' || *p == L'\t') ++p;
    if (lstrcmpW(p, L"--verify")) { say("Usage: msdc-file-verify.exe --verify\r\n"); ExitProcess(ERROR_INVALID_PARAMETER); }
    say("MSDC_FILE_VERIFY_BEGIN\r\n"); error = verify();
    say("MSDC_FILE_VERIFY_END\r\n");
    ExitProcess(error);
}
