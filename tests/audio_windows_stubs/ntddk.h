#ifndef AUDIO_TEST_NTDDK_H
#define AUDIO_TEST_NTDDK_H
#include <stdint.h>
#if defined(A04_AUDIO_DIAGNOSTIC_TRACE) && A04_AUDIO_DIAGNOSTIC_TRACE
#define DPFLTR_IHVDRIVER_ID 0
#define DPFLTR_INFO_LEVEL 0
extern int a04_test_trace_count;
static inline int DbgPrintEx(unsigned a,unsigned b,const char *fmt,...){(void)a;(void)b;(void)fmt;a04_test_trace_count++;return 0;}
#endif

#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
#include <assert.h>
#endif
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
inline uint32_t READ_REGISTER_ULONG(volatile uint32_t *p) { return *p; }
inline void WRITE_REGISTER_ULONG(volatile uint32_t *p,uint32_t value) { *p=value; }
#endif
typedef void *PVOID; typedef uint32_t ULONG; typedef unsigned char BOOLEAN,KIRQL; typedef uintptr_t KAFFINITY; typedef int32_t NTSTATUS,LONG; typedef enum { LevelSensitive, Latched } KINTERRUPT_MODE;
#ifdef A04_AUDIO_QUERY_SHARED
typedef uint16_t USHORT;
typedef struct { uint32_t a; uint16_t b,c; uint8_t d[8]; } GUID;
typedef void (*PINTERFACE_REFERENCE)(PVOID);
typedef struct { USHORT Size,Version; PVOID Context; PINTERFACE_REFERENCE InterfaceReference,InterfaceDereference; } INTERFACE,*PINTERFACE;
#endif
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
#if defined(A04_AUDIO_IRQ_DIAGNOSTIC) && A04_AUDIO_IRQ_DIAGNOSTIC
extern int test_connect_fail,test_irq_connections,test_irq_disconnections;
void test_before_disconnect();
inline NTSTATUS IoConnectInterrupt(PKINTERRUPT *p,ISR_FUNC f,PVOID c,void *,ULONG v,KIRQL l,KIRQL,KINTERRUPT_MODE,BOOLEAN,KAFFINITY a,BOOLEAN) { assert(v==80 && l==5 && a==1);if(test_connect_fail)return -1;test_irq_connections++;test_interrupt.fn=f;test_interrupt.ctx=c;*p=&test_interrupt;return 0; }
inline void IoDisconnectInterrupt(PKINTERRUPT) { test_before_disconnect();test_irq_disconnections++;test_interrupt.fn=0; }
#else
inline NTSTATUS IoConnectInterrupt(PKINTERRUPT *p,ISR_FUNC f,PVOID c,void *,ULONG,KIRQL,KIRQL,KINTERRUPT_MODE,BOOLEAN,KAFFINITY,BOOLEAN) { test_interrupt.fn=f;test_interrupt.ctx=c;*p=&test_interrupt;return 0; }
inline void IoDisconnectInterrupt(PKINTERRUPT) { test_interrupt.fn=0; }
#endif
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
#include <sched.h>
#define PASSIVE_LEVEL 0
#define Executive 0
#define KernelMode 0
extern thread_local int test_audio_irql;
typedef struct { volatile int held; } KMUTEX;
inline int KeGetCurrentIrql(){return test_audio_irql;}
inline void KeInitializeMutex(KMUTEX *m,int){m->held=0;}
inline NTSTATUS KeWaitForSingleObject(KMUTEX *m,int,int,BOOLEAN,void *){assert(test_audio_irql==0);while(__sync_lock_test_and_set(&m->held,1))sched_yield();return STATUS_SUCCESS;}
inline int KeReleaseMutex(KMUTEX *m,BOOLEAN){__sync_lock_release(&m->held);return 0;}
typedef struct { volatile int held; } FAST_MUTEX;
typedef struct { volatile int state; } EX_RUNDOWN_REF;
inline void ExInitializeRundownProtection(EX_RUNDOWN_REF *r){r->state=0;}
inline BOOLEAN ExAcquireRundownProtection(EX_RUNDOWN_REF *r){int v;do{v=__sync_fetch_and_add(&r->state,0);if(v&0x40000000)return FALSE;}while(!__sync_bool_compare_and_swap(&r->state,v,v+1));return TRUE;}
inline void ExReleaseRundownProtection(EX_RUNDOWN_REF *r){__sync_fetch_and_sub(&r->state,1);}
inline void ExWaitForRundownProtectionRelease(EX_RUNDOWN_REF *r){__sync_fetch_and_or(&r->state,0x40000000);while(__sync_fetch_and_add(&r->state,0)&0x3fffffff)sched_yield();}
inline void ExInitializeFastMutex(FAST_MUTEX *m){m->held=0;}
inline void ExAcquireFastMutex(FAST_MUTEX *m){while(__sync_lock_test_and_set(&m->held,1))sched_yield();}
inline void ExReleaseFastMutex(FAST_MUTEX *m){__sync_lock_release(&m->held);}
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
