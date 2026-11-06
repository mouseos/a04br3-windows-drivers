/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Host-only minimal contracts for GPU lifecycle tests. */
#ifndef A04_HOST_NTDDK_H
#define A04_HOST_NTDDK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint8_t UCHAR, BOOLEAN; typedef uint32_t ULONG; typedef int32_t LONG; typedef uintptr_t ULONG_PTR; typedef uint64_t ULONGLONG;
typedef int32_t NTSTATUS;
#ifdef A04_TEST_DISPLAY_SCANOUT
typedef uintptr_t PFN_NUMBER; typedef size_t SIZE_T; typedef void *PVOID;
typedef struct fake_mdl { unsigned short MdlFlags; ULONG offset,bytes; void *va; PFN_NUMBER *pfns; } *PMDL;
typedef void *PDEVICE_OBJECT, *PIRP;

#else
typedef void *PMDL;
#endif
typedef union { int64_t QuadPart; } PHYSICAL_ADDRESS, LARGE_INTEGER;
#ifdef A04_TEST_DISPLAY_SCANOUT
typedef struct { PHYSICAL_ADDRESS Address; ULONG Length; } SCATTER_GATHER_ELEMENT;
typedef struct { ULONG NumberOfElements; SCATTER_GATHER_ELEMENT Elements[4]; } SCATTER_GATHER_LIST, *PSCATTER_GATHER_LIST;
typedef struct { int signaled; } KEVENT;
typedef void (*PDRIVER_LIST_CONTROL)(PDEVICE_OBJECT,PIRP,PSCATTER_GATHER_LIST,PVOID);
#define MDL_PAGES_LOCKED 1
#define MmGetMdlByteOffset(m) ((m)->offset)
#define MmGetMdlByteCount(m) ((m)->bytes)
#define MmGetMdlPfnArray(m) ((m)->pfns)
#define MmGetMdlVirtualAddress(m) ((m)->va)
#define NotificationEvent 0
#define Executive 0
#define IO_NO_INCREMENT 0
void KeInitializeEvent(KEVENT *,int,BOOLEAN);
void KeSetEvent(KEVENT *,int,BOOLEAN);
NTSTATUS KeWaitForSingleObject(void *,int,int,BOOLEAN,void *);
PHYSICAL_ADDRESS MmGetPhysicalAddress(void *);
#endif
typedef struct fake_adapter *PDMA_ADAPTER;
typedef struct {
    void *(*AllocateCommonBuffer)(PDMA_ADAPTER,ULONG,PHYSICAL_ADDRESS *,BOOLEAN);
    void (*FreeCommonBuffer)(PDMA_ADAPTER,ULONG,PHYSICAL_ADDRESS,void *,BOOLEAN);
#ifdef A04_TEST_DISPLAY_SCANOUT
    NTSTATUS (*GetScatterGatherList)(PDMA_ADAPTER,PDEVICE_OBJECT,PMDL,void *,ULONG,PDRIVER_LIST_CONTROL,void *,BOOLEAN);
    void (*PutScatterGatherList)(PDMA_ADAPTER,PSCATTER_GATHER_LIST,BOOLEAN);
#endif
    ULONG Size;
    void *(*AllocateCommonBufferEx)(PDMA_ADAPTER,PHYSICAL_ADDRESS *,ULONG,PHYSICAL_ADDRESS *,BOOLEAN,ULONG);
} DMA_OPERATIONS;
struct fake_adapter { DMA_OPERATIONS *DmaOperations; };
#define STATUS_SUCCESS ((NTSTATUS)0)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_DATA_ERROR ((NTSTATUS)0xc000003e)
#define STATUS_INVALID_DEVICE_STATE ((NTSTATUS)0xc0000184)
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xc000009a)
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#define STATUS_INVALID_IMAGE_FORMAT ((NTSTATUS)0xc000007b)
#define STATUS_DEVICE_CONFIGURATION_ERROR ((NTSTATUS)0xc0000182)
#define STATUS_IO_TIMEOUT ((NTSTATUS)0xc00000b5)
#define STATUS_DEVICE_BUSY ((NTSTATUS)0x80000011)
#define STATUS_IO_DEVICE_ERROR ((NTSTATUS)0xc0000185)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define NT_SUCCESS(x) ((x)>=0)
#define FALSE 0
#define TRUE 1
#define NonPagedPoolNx 0
void *ExAllocatePoolWithTag(int,size_t,unsigned);
void ExFreePoolWithTag(void *,unsigned);
#define PASSIVE_LEVEL 0
#define DISPATCH_LEVEL 2
#define KernelMode 0
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#define RtlZeroMemory(p,n) memset((p),0,(n))
#define RtlCopyMemory(d,s,n) memcpy((d),(s),(n))
#define RtlFillMemory(p,n,v) memset((p),(v),(n))
#define READ_REGISTER_ULONG(p) (*(p))
#define WRITE_REGISTER_ULONG(p,v) (*(p)=(v))
#define KeMemoryBarrier() ((void)0)
#ifdef A04_TEST_INTERLOCK_HOOK
void a04_test_interlock_hook(volatile LONG *p,LONG value,LONG expected);
#endif
static inline LONG InterlockedCompareExchange(volatile LONG *p,LONG value,LONG expected)
{
#ifdef A04_TEST_INTERLOCK_HOOK
    a04_test_interlock_hook(p,value,expected);
#endif
    return __sync_val_compare_and_swap(p,expected,value);
}
static inline LONG InterlockedExchange(volatile LONG *p,LONG value)
{ return __sync_lock_test_and_set(p,value); }
unsigned KeGetCurrentIrql(void);
void KeStallExecutionProcessor(unsigned);
NTSTATUS KeDelayExecutionThread(int,BOOLEAN,const LARGE_INTEGER *);
PMDL IoAllocateMdl(void *,ULONG,BOOLEAN,BOOLEAN,void *);
void MmBuildMdlForNonPagedPool(PMDL);
void KeFlushIoBuffers(PMDL,BOOLEAN,BOOLEAN);
void IoFreeMdl(PMDL);
#endif
