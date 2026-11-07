#ifndef A04_CHECKPOINT_TEST_NTDDK_H
#define A04_CHECKPOINT_TEST_NTDDK_H
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
#include <string.h>
typedef void *HANDLE,*PVOID;typedef const wchar_t *PCWSTR;typedef uint8_t UCHAR;typedef uint32_t ULONG;typedef int32_t NTSTATUS;
typedef struct {int held;} KMUTEX;
typedef struct {int64_t QuadPart;} LARGE_INTEGER;
typedef struct {PCWSTR text;} UNICODE_STRING;
typedef struct {UNICODE_STRING *name;HANDLE root;ULONG flags;} OBJECT_ATTRIBUTES;
typedef struct {ULONG TitleIndex,Type,DataLength;UCHAR Data[1];} KEY_VALUE_PARTIAL_INFORMATION;
#define FIELD_OFFSET(t,m) ((ULONG)offsetof(t,m))
#define PASSIVE_LEVEL 0
#define KernelMode 0
#define Executive 0
#define FALSE 0
#define PagedPool 1
#define STATUS_SUCCESS ((NTSTATUS)0)
#define STATUS_INVALID_DEVICE_STATE ((NTSTATUS)0xc0000184u)
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xc000009au)
#define STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xc0000034u)
#define STATUS_DATA_ERROR ((NTSTATUS)0xc000003eu)
#define STATUS_INTEGER_OVERFLOW ((NTSTATUS)0xc0000095u)
#define STATUS_RETRY ((NTSTATUS)0xc000022du)
#define STATUS_QUOTA_EXCEEDED ((NTSTATUS)0xc0000044u)
#define NT_SUCCESS(s) ((NTSTATUS)(s)>=0)
#define OBJ_CASE_INSENSITIVE 64u
#define OBJ_KERNEL_HANDLE 512u
#define KEY_CREATE_SUB_KEY 4u
#define KEY_QUERY_VALUE 1u
#define KEY_WRITE 0x20006u
#define REG_BINARY 3u
#define REG_QWORD 11u
#define REG_OPTION_NON_VOLATILE 0
#define KeyValuePartialInformation 2u
#define RtlZeroMemory(p,n) memset(p,0,n)
#define RtlCopyMemory(p,q,n) memcpy(p,q,n)
#define InitializeObjectAttributes(a,n,f,r,s) do{(a)->name=(n);(a)->flags=(f);(a)->root=(r);(void)(s);}while(0)
int KeGetCurrentIrql(void);void KeInitializeMutex(KMUTEX *,ULONG);
NTSTATUS KeWaitForSingleObject(void *,int,int,int,void *);int KeReleaseMutex(KMUTEX *,int);
void RtlInitUnicodeString(UNICODE_STRING *,PCWSTR);
PVOID ExAllocatePoolWithTag(int,size_t,ULONG);void ExFreePoolWithTag(PVOID,ULONG);
uint64_t KeQueryInterruptTime(void);void KeQuerySystemTime(LARGE_INTEGER *);
NTSTATUS ZwOpenKey(HANDLE *,ULONG,OBJECT_ATTRIBUTES *);
NTSTATUS ZwCreateKey(HANDLE *,ULONG,OBJECT_ATTRIBUTES *,ULONG,UNICODE_STRING *,ULONG,ULONG *);
NTSTATUS ZwClose(HANDLE);
NTSTATUS ZwSetValueKey(HANDLE,UNICODE_STRING *,ULONG,ULONG,PVOID,ULONG);
NTSTATUS ZwQueryValueKey(HANDLE,UNICODE_STRING *,ULONG,PVOID,ULONG,ULONG *);
NTSTATUS ZwFlushKey(HANDLE);
#endif
