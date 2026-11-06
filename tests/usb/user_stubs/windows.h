#ifndef FAKE_WINDOWS_H
#define FAKE_WINDOWS_H
#include <stdint.h>
#include <stddef.h>
typedef uint32_t ULONG,DWORD;typedef void *HANDLE;
#define NULL ((void *)0)
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define GENERIC_WRITE 2
#define GENERIC_READ 1
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING 3
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define ERROR_BUSY 170
#define ERROR_NOT_READY 21
#define ERROR_INVALID_DATA 13
#define FILE_DEVICE_UNKNOWN 0x22
#define METHOD_BUFFERED 0
#define FILE_READ_ACCESS 1
#define FILE_WRITE_ACCESS 2
#define CTL_CODE(d,f,m,a) (((d)<<16)|((a)<<14)|((f)<<2)|(m))
HANDLE CreateFileW(const wchar_t *,DWORD,DWORD,void *,DWORD,DWORD,HANDLE);
int DeviceIoControl(HANDLE,DWORD,void *,DWORD,void *,DWORD,DWORD *,void *);
int CloseHandle(HANDLE);DWORD GetTickCount(void);DWORD GetLastError(void);void Sleep(DWORD);
HANDLE GetStdHandle(DWORD);int WriteFile(HANDLE,const void *,DWORD,DWORD *,void *);
void ExitProcess(unsigned);
#endif
