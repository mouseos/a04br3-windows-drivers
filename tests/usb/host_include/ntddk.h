/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Linux host type seam for the existing deterministic USB tests only.
 * Not WDK ABI validation; all OS operations are supplied by those tests. */
#ifndef MT8167_USB_HOST_NTDDK_H
#define MT8167_USB_HOST_NTDDK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef void VOID,*PVOID;typedef uint8_t UCHAR,BOOLEAN,KIRQL;typedef uint16_t USHORT,*PUSHORT;
typedef uint32_t ULONG,*PULONG;typedef int32_t LONG,NTSTATUS;typedef uint64_t ULONGLONG;
typedef int64_t LONGLONG;typedef uintptr_t ULONG_PTR,KAFFINITY;typedef size_t SIZE_T;
typedef union {int64_t QuadPart;} LARGE_INTEGER,PHYSICAL_ADDRESS;typedef LARGE_INTEGER *PLARGE_INTEGER;
typedef unsigned KSPIN_LOCK,KTIMER,KEVENT,KDPC,FAST_MUTEX;typedef KDPC *PKDPC;
typedef void *PDEVICE_OBJECT,*PDRIVER_OBJECT,*PIO_WORKITEM;typedef struct {int unused;} UNICODE_STRING;typedef UNICODE_STRING *PUNICODE_STRING;
typedef int KPROCESSOR_MODE,POOL_TYPE,MEMORY_CACHING_TYPE,WORK_QUEUE_TYPE;
typedef VOID (*PIO_WORKITEM_ROUTINE)(PDEVICE_OBJECT,PVOID);
typedef struct {uint32_t a;uint16_t b,c;uint8_t d[8];} GUID;
typedef VOID (*PINTERFACE_REFERENCE)(PVOID);
typedef struct {USHORT Size,Version;PVOID Context;PINTERFACE_REFERENCE InterfaceReference,InterfaceDereference;} INTERFACE;
#define NTKERNELAPI
#define NT_SUCCESS(s) ((NTSTATUS)(s)>=0)
#define STATUS_SUCCESS ((NTSTATUS)0)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define STATUS_DEVICE_HARDWARE_ERROR ((NTSTATUS)0xc0000185)
#define STATUS_DEVICE_POWER_FAILURE ((NTSTATUS)0xc000009e)
#define STATUS_DEVICE_BUSY ((NTSTATUS)0x80000011)
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#define STATUS_INVALID_DEVICE_STATE ((NTSTATUS)0xc0000184)
#define STATUS_DEVICE_CONFIGURATION_ERROR ((NTSTATUS)0xc0000182)
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xc000009a)
#define TRUE 1
#define FALSE 0
#define PASSIVE_LEVEL 0
#define DISPATCH_LEVEL 2
#define KernelMode 0
#define Executive 0
#define NotificationEvent 0
#define IO_NO_INCREMENT 0
#define NonPagedPoolNx 0
#define MmNonCached 0
#define DelayedWorkQueue 0
#define UNREFERENCED_PARAMETER(p) ((void)(p))
#define RtlZeroMemory(p,n) memset(p,0,n)
#define RtlCopyMemory(d,s,n) memcpy(d,s,n)
#define FIELD_OFFSET(t,f) offsetof(t,f)
#define C_ASSERT(x) _Static_assert(x,#x)
ULONGLONG KeQueryInterruptTime(void);
KIRQL KeGetCurrentIrql(void);
LONG InterlockedIncrement(volatile LONG *);
#endif
