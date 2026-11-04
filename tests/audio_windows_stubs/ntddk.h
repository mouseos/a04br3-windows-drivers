#ifndef AUDIO_TEST_NTDDK_H
#define AUDIO_TEST_NTDDK_H
#include <stdint.h>
typedef void *PVOID; typedef uint32_t ULONG; typedef unsigned char BOOLEAN,KIRQL; typedef uintptr_t KAFFINITY; typedef int32_t NTSTATUS,LONG; typedef enum { LevelSensitive, Latched } KINTERRUPT_MODE;
#define FALSE 0
#define TRUE 1
#ifndef NULL
#define NULL 0
#endif
#define STATUS_INVALID_PARAMETER (-1)
#define NT_SUCCESS(s) ((s)>=0)
#define UNREFERENCED_PARAMETER(x) ((void)(x))
struct _KINTERRUPT; typedef _KINTERRUPT *PKINTERRUPT;
struct _KDPC; typedef _KDPC KDPC, *PKDPC;
typedef void (*DPC_FUNC)(PKDPC,PVOID,PVOID,PVOID);
struct _KDPC { DPC_FUNC fn; PVOID ctx; int queued; };
typedef BOOLEAN (*ISR_FUNC)(PKINTERRUPT,PVOID);
struct _KINTERRUPT { ISR_FUNC fn; PVOID ctx; };
extern _KINTERRUPT test_interrupt;
inline LONG InterlockedCompareExchange(volatile LONG *p,LONG a,LONG b) { LONG o=*p;if(o==b)*p=a;return o; }
inline LONG InterlockedExchange(volatile LONG *p,LONG a) { LONG o=*p;*p=a;return o; }
#ifdef A04_TEST_PNP_BACKEND
inline LONG InterlockedIncrement(volatile LONG *p) { return ++*p; }
inline LONG InterlockedDecrement(volatile LONG *p) { return --*p; }
#endif
inline void KeInitializeDpc(PKDPC d,DPC_FUNC f,PVOID c) { d->fn=f;d->ctx=c;d->queued=0; }
inline BOOLEAN KeInsertQueueDpc(PKDPC d,PVOID,PVOID) { if(d->queued)return FALSE;d->queued=1;return TRUE; }
inline BOOLEAN KeRemoveQueueDpc(PKDPC d) { int q=d->queued;d->queued=0;return q; }
inline void KeFlushQueuedDpcs() { }
inline BOOLEAN KeSynchronizeExecution(PKINTERRUPT, BOOLEAN (*fn)(PVOID), PVOID c) { return fn(c); }
inline NTSTATUS IoConnectInterrupt(PKINTERRUPT *p,ISR_FUNC f,PVOID c,void *,ULONG,KIRQL,KIRQL,KINTERRUPT_MODE,BOOLEAN,KAFFINITY,BOOLEAN) { test_interrupt.fn=f;test_interrupt.ctx=c;*p=&test_interrupt;return 0; }
inline void IoDisconnectInterrupt(PKINTERRUPT) { test_interrupt.fn=0; }
#ifdef A04_TEST_COMMON_DMA
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
typedef uint64_t ULONGLONG;
typedef ULONG *PULONG; typedef void *PDEVICE_OBJECT;
typedef struct { ULONG Version; BOOLEAN Master,ScatterGather,Dma32BitAddresses; int InterfaceType; ULONG MaximumLength; } DEVICE_DESCRIPTION;
#define DEVICE_DESCRIPTION_VERSION 0
#define Internal 1
#define STATUS_NOT_SUPPORTED (-5)
typedef struct {int64_t QuadPart;} PHYSICAL_ADDRESS;
struct _MDL { void *address; ULONG size; }; typedef _MDL *PMDL;
struct _DMA_ADAPTER; typedef _DMA_ADAPTER *PDMA_ADAPTER;
struct _DMA_OPERATIONS {
 void (*PutDmaAdapter)(PDMA_ADAPTER);
 PVOID (*AllocateCommonBuffer)(PDMA_ADAPTER,ULONG,PHYSICAL_ADDRESS*,BOOLEAN);
 void (*FreeCommonBuffer)(PDMA_ADAPTER,ULONG,PHYSICAL_ADDRESS,PVOID,BOOLEAN);
};
struct _DMA_ADAPTER { _DMA_OPERATIONS *DmaOperations; };
#define PAGE_SIZE 4096u
#define STATUS_INSUFFICIENT_RESOURCES (-2)
#define STATUS_DEVICE_CONFIGURATION_ERROR (-3)
#define STATUS_DEVICE_BUSY (-4)
#define STATUS_SUCCESS 0
#define RtlZeroMemory(p,n) memset(p,0,n)
extern int test_mdl_fail, test_mdl_frees, test_barriers, test_flushes;
extern PDMA_ADAPTER test_dma_adapter;
extern DEVICE_DESCRIPTION test_dma_description;
inline PDMA_ADAPTER IoGetDmaAdapter(PDEVICE_OBJECT,DEVICE_DESCRIPTION *d,PULONG n) { test_dma_description=*d;*n=4;return test_dma_adapter; }
inline PMDL IoAllocateMdl(PVOID p,ULONG n,BOOLEAN,BOOLEAN,PVOID) { if(test_mdl_fail)return NULL; PMDL m=(PMDL)malloc(sizeof(*m)); m->address=p;m->size=n;return m; }
inline void MmBuildMdlForNonPagedPool(PMDL) {}
inline void IoFreeMdl(PMDL m) {test_mdl_frees++;free(m);}
inline void KeMemoryBarrier() {test_barriers++;}
inline void KeFlushIoBuffers(PMDL,BOOLEAN read,BOOLEAN dma) { if (!read && dma) test_flushes++; }
#ifdef A04_TEST_PNP_BACKEND
#include <assert.h>
typedef struct { int held; } FAST_MUTEX;
inline void ExInitializeFastMutex(FAST_MUTEX *m){m->held=0;}
inline void ExAcquireFastMutex(FAST_MUTEX *m){assert(!m->held);m->held=1;}
inline void ExReleaseFastMutex(FAST_MUTEX *m){assert(m->held);m->held=0;}
#define NonPagedPool 0
inline PVOID ExAllocatePoolWithTag(int,size_t n,ULONG){return malloc(n);}
inline void ExFreePoolWithTag(PVOID p,ULONG){free(p);}
extern int test_pdo_refs;
inline void ObReferenceObject(PDEVICE_OBJECT){test_pdo_refs++;}
inline void ObDereferenceObject(PDEVICE_OBJECT){test_pdo_refs--;}
#define STATUS_DEVICE_NOT_READY (-6)
#endif
#endif
#endif
