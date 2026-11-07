#ifndef A04_TEST_FIRMWARE_NTDDK_H
#define A04_TEST_FIRMWARE_NTDDK_H
#include "../shared_wdf_stubs/ntddk.h"
typedef wchar_t WCHAR;typedef void *HANDLE;
typedef struct {NTSTATUS Status;size_t Information;} IO_STATUS_BLOCK;
typedef struct {struct {int64_t QuadPart;} EndOfFile;int Directory;} FILE_STANDARD_INFORMATION;
typedef struct {int dummy;} OBJECT_ATTRIBUTES;
#define STATUS_INVALID_BUFFER_SIZE (-6)
#define STATUS_END_OF_FILE (-7)
#define OBJ_KERNEL_HANDLE 1
#define OBJ_CASE_INSENSITIVE 2
#define FILE_READ_DATA 1
#define FILE_READ_ATTRIBUTES 2
#define SYNCHRONIZE 4
#define FILE_ATTRIBUTE_NORMAL 0
#define FILE_SHARE_READ 1
#define FILE_OPEN 1
#define FILE_NON_DIRECTORY_FILE 1
#define FILE_SYNCHRONOUS_IO_NONALERT 2
#define FileStandardInformation 1
#define PagedPool 1
#define InitializeObjectAttributes(o,n,f,r,s) do {(void)(o);(void)(n);(void)(f);(void)(r);(void)(s);}while(0)
#define RtlSecureZeroMemory(p,n) memset(p,0,n)
NTSTATUS ZwCreateFile(HANDLE *,ULONG,OBJECT_ATTRIBUTES *,IO_STATUS_BLOCK *,void *,ULONG,ULONG,ULONG,ULONG,void *,ULONG);
NTSTATUS ZwQueryInformationFile(HANDLE,IO_STATUS_BLOCK *,void *,ULONG,int);
NTSTATUS ZwReadFile(HANDLE,void *,void *,void *,IO_STATUS_BLOCK *,void *,ULONG,void *,void *);
NTSTATUS ZwClose(HANDLE);
void *ExAllocatePoolWithTag(int,size_t,ULONG);
void ExFreePoolWithTag(void *,ULONG);
#endif
