/* SPDX-License-Identifier: BSD-2-Clause-Patent -- host brightness lifecycle contracts. */
#ifndef A04_BRIGHTNESS_TEST_NTDDK_H
#define A04_BRIGHTNESS_TEST_NTDDK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef void VOID,*PVOID;typedef uint8_t UCHAR,*PUCHAR,BOOLEAN;typedef uint16_t USHORT;typedef uint32_t ULONG;typedef int32_t LONG,NTSTATUS;
typedef struct {uint32_t Data1;uint16_t Data2,Data3;uint8_t Data4[8];} GUID;
typedef struct {int64_t QuadPart;} PHYSICAL_ADDRESS;
typedef struct {int held;} KMUTEX;
typedef struct {UCHAR Type,ShareDisposition;USHORT Flags;union {struct {PHYSICAL_ADDRESS Start;ULONG Length;} Memory;} u;} CM_PARTIAL_RESOURCE_DESCRIPTOR,*PCM_PARTIAL_RESOURCE_DESCRIPTOR;
typedef struct {USHORT Version,Revision;ULONG Count;CM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptors[8];} CM_PARTIAL_RESOURCE_LIST,*PCM_PARTIAL_RESOURCE_LIST;
typedef struct {ULONG InterfaceType,BusNumber;CM_PARTIAL_RESOURCE_LIST PartialResourceList;} CM_FULL_RESOURCE_DESCRIPTOR,*PCM_FULL_RESOURCE_DESCRIPTOR;
typedef struct {ULONG Count;CM_FULL_RESOURCE_DESCRIPTOR List[1];} CM_RESOURCE_LIST,*PCM_RESOURCE_LIST;
#define TRUE 1
#define FALSE 0
#define MAXULONG 0xffffffffu
#define PASSIVE_LEVEL 0
#define Executive 0
#define KernelMode 0
#define NonPagedPoolNx 0
#define MmNonCached 0
#define CmResourceTypeMemory 3
#define CmResourceShareDeviceExclusive 1
#define CM_RESOURCE_MEMORY_READ_ONLY 1
#define STATUS_SUCCESS ((NTSTATUS)0)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define STATUS_DEVICE_CONFIGURATION_ERROR ((NTSTATUS)0xc0000182)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xc0000023)
#define STATUS_IO_DEVICE_ERROR ((NTSTATUS)0xc0000185)
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xc000009a)
#define NT_SUCCESS(s) ((NTSTATUS)(s)>=0)
#define RtlZeroMemory(p,n) memset(p,0,n)
static inline size_t RtlCompareMemory(const void*a,const void*b,size_t n){const UCHAR*x=(const UCHAR*)a,*y=(const UCHAR*)b;size_t i=0;while(i<n && x[i]==y[i])i++;return i;}
static inline LONG InterlockedIncrement(volatile LONG*p){return __sync_add_and_fetch(p,1);}
static inline LONG InterlockedDecrement(volatile LONG*p){return __sync_sub_and_fetch(p,1);}
#define KeMemoryBarrier() __sync_synchronize()
ULONG KeGetCurrentIrql(void);
void KeInitializeMutex(KMUTEX*,ULONG);
NTSTATUS KeWaitForSingleObject(void*,int,int,BOOLEAN,void*);
void KeReleaseMutex(KMUTEX*,BOOLEAN);
void* ExAllocatePoolWithTag(int,size_t,ULONG);
void ExFreePoolWithTag(void*,ULONG);
void* MmMapIoSpace(PHYSICAL_ADDRESS,size_t,int);
void MmUnmapIoSpace(void*,size_t);
ULONG READ_REGISTER_ULONG(volatile ULONG*);
void WRITE_REGISTER_ULONG(volatile ULONG*,ULONG);
#endif
