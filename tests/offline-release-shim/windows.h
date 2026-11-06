#ifndef A04_TEST_WINDOWS_H
#define A04_TEST_WINDOWS_H
#include <stddef.h>
#include <stdint.h>
typedef unsigned char BYTE; typedef struct { long long QuadPart; } LARGE_INTEGER; typedef struct { uint32_t Data1; uint16_t Data2,Data3; BYTE Data4[8]; } GUID;
typedef uint64_t ULONGLONG; typedef uint32_t DWORD; typedef unsigned int UINT; typedef int BOOL; typedef int32_t LONG; typedef wchar_t WCHAR;
typedef void *HANDLE; typedef void *HKEY;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INVALID_FILE_ATTRIBUTES 0xffffffffu
#define ERROR_SUCCESS 0
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_NO_MORE_FILES 18
#define ERROR_NOT_READY 21
#define FILE_BEGIN 0
#define TRUE 1
#define CSTR_EQUAL 2
#define FILE_ATTRIBUTE_HIDDEN 2
#define FILE_ATTRIBUTE_SYSTEM 4
#define FILE_ATTRIBUTE_DIRECTORY 16
#define FILE_ATTRIBUTE_REPARSE_POINT 1024
#define KEY_READ 1
#define HKEY_LOCAL_MACHINE ((HKEY)1)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define GENERIC_READ 1
#define GENERIC_WRITE 2
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING 3
HANDLE GetStdHandle(DWORD); BOOL WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
LONG RegOpenKeyExW(HKEY,const WCHAR*,DWORD,DWORD,HKEY*); LONG RegCloseKey(HKEY);
BOOL GetVolumeInformationW(const WCHAR*,WCHAR*,DWORD,DWORD*,DWORD*,DWORD*,WCHAR*,DWORD);
int lstrcmpiW(const WCHAR*,const WCHAR*); DWORD GetFileAttributesW(const WCHAR*);
HANDLE CreateFileW(const WCHAR*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
BOOL DeviceIoControl(HANDLE,DWORD,void*,DWORD,void*,DWORD,DWORD*,void*);
BOOL ReadFile(HANDLE,void*,DWORD,DWORD*,void*); BOOL SetFilePointerEx(HANDLE,LARGE_INTEGER,LARGE_INTEGER*,DWORD); void Sleep(DWORD); int CompareStringOrdinal(const WCHAR*,int,const WCHAR*,int,BOOL);
BOOL FlushFileBuffers(HANDLE); BOOL CloseHandle(HANDLE); DWORD GetLastError(void);
BOOL DeleteVolumeMountPointW(const WCHAR*); DWORD GetLogicalDrives(void);
UINT GetWindowsDirectoryW(WCHAR*,UINT);
int lstrlenW(const WCHAR*); WCHAR *lstrcpyW(WCHAR*,const WCHAR*); WCHAR *lstrcatW(WCHAR*,const WCHAR*);
BOOL GetVolumeNameForVolumeMountPointW(const WCHAR*,WCHAR*,DWORD); BOOL SetVolumeMountPointW(const WCHAR*,const WCHAR*);
HANDLE FindFirstVolumeW(WCHAR*,DWORD); BOOL FindNextVolumeW(HANDLE,WCHAR*,DWORD); BOOL FindVolumeClose(HANDLE);
void ExitProcess(DWORD);
#endif
