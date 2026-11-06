#ifndef WINDOWS_H
#define WINDOWS_H
#include <stdint.h>
#include <stddef.h>
typedef uint32_t DWORD; typedef int BOOL; typedef long LONG; typedef wchar_t WCHAR;
typedef void *HANDLE; typedef void *HKEY;
#define NULL ((void *)0)
#define ERROR_SUCCESS 0
#define ERROR_FILE_NOT_FOUND 2
#define KEY_READ 0x20019
#define GENERIC_WRITE 0x40000000
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING 3
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define HKEY_LOCAL_MACHINE ((HKEY)(intptr_t)-2147483646)
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
LONG RegOpenKeyExW(HKEY,const WCHAR *,DWORD,DWORD,HKEY *);
LONG RegCloseKey(HKEY);
HANDLE CreateFileW(const WCHAR *,DWORD,DWORD,void *,DWORD,DWORD,HANDLE);
BOOL FlushFileBuffers(HANDLE); BOOL CloseHandle(HANDLE);
HANDLE GetStdHandle(DWORD); BOOL WriteFile(HANDLE,const void *,DWORD,DWORD *,void *);
void ExitProcess(DWORD);
#endif
