#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
typedef int32_t NTSTATUS,LONG;typedef uint32_t ULONG;typedef uint16_t USHORT;typedef uint8_t UCHAR;typedef uint64_t ULONGLONG;typedef int64_t LONGLONG;typedef void *PVOID;typedef void VOID;
typedef struct{uint32_t a;uint16_t b,c;uint8_t d[8];}GUID;
typedef struct{int64_t QuadPart;}PHYSICAL_ADDRESS,LARGE_INTEGER;
typedef struct{const wchar_t*Buffer;}UNICODE_STRING;
#define RTL_CONSTANT_STRING(x) {x}
#define TRUE 1
#define FALSE 0
#define PASSIVE_LEVEL 0
#define APC_LEVEL 1
#define KernelMode 0
#define MmNonCached 0
#define MAXULONG UINT32_MAX
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define STATUS_DEVICE_BUSY ((NTSTATUS)0x80000011)
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xc000009a)
#define STATUS_DEVICE_HARDWARE_ERROR ((NTSTATUS)0xc0000185)
#define STATUS_IO_TIMEOUT ((NTSTATUS)0xc00000b5)
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#define NT_SUCCESS(x) ((x)>=0)
#define RtlZeroMemory(p,n) memset(p,0,n)
#define UNREFERENCED_PARAMETER(x) (void)(x)
#define DPFLTR_IHVDRIVER_ID 0
#define DPFLTR_ERROR_LEVEL 0
#define DbgPrintEx(...) ((void)0)
struct parent;typedef struct parent*WDFDEVICE;typedef WDFDEVICE WDFQUEUE;
struct request;typedef struct request*WDFREQUEST;
typedef struct{USHORT Size,Version;PVOID Context;void(*InterfaceReference)(PVOID);void(*InterfaceDereference)(PVOID);}INTERFACE;
static LONG InterlockedCompareExchange(volatile LONG*p,LONG value,LONG compare){LONG old=*p;if(old==compare)*p=value;return old;}
static LONG InterlockedExchange(volatile LONG*p,LONG value){LONG old=*p;*p=value;return old;}
