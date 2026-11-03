/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Host-only minimal contracts for GPU lifecycle tests. */
#ifndef A04_HOST_NTDDK_H
#define A04_HOST_NTDDK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint8_t UCHAR, BOOLEAN; typedef uint32_t ULONG; typedef uintptr_t ULONG_PTR; typedef uint64_t ULONGLONG;
typedef int32_t NTSTATUS;
typedef union { int64_t QuadPart; } PHYSICAL_ADDRESS, LARGE_INTEGER;
typedef struct fake_adapter *PDMA_ADAPTER;
typedef struct {
    void *(*AllocateCommonBuffer)(PDMA_ADAPTER,ULONG,PHYSICAL_ADDRESS *,BOOLEAN);
    void (*FreeCommonBuffer)(PDMA_ADAPTER,ULONG,PHYSICAL_ADDRESS,void *,BOOLEAN);
} DMA_OPERATIONS;
struct fake_adapter { DMA_OPERATIONS *DmaOperations; };
#define STATUS_SUCCESS ((NTSTATUS)0)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_INVALID_DEVICE_STATE ((NTSTATUS)0xc0000184)
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xc000009a)
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#define STATUS_INVALID_IMAGE_FORMAT ((NTSTATUS)0xc000007b)
#define STATUS_DEVICE_CONFIGURATION_ERROR ((NTSTATUS)0xc0000182)
#define STATUS_IO_TIMEOUT ((NTSTATUS)0xc00000b5)
#define NT_SUCCESS(x) ((x)>=0)
#define FALSE 0
#define TRUE 1
#define PASSIVE_LEVEL 0
#define KernelMode 0
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#define RtlZeroMemory(p,n) memset((p),0,(n))
#define READ_REGISTER_ULONG(p) (*(p))
#define WRITE_REGISTER_ULONG(p,v) (*(p)=(v))
#define KeMemoryBarrier() ((void)0)
unsigned KeGetCurrentIrql(void);
void KeStallExecutionProcessor(unsigned);
NTSTATUS KeDelayExecutionThread(int,BOOLEAN,const LARGE_INTEGER *);
#endif
