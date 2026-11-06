#ifndef WAVE_PROBE_TEST_NTDDK_H
#define WAVE_PROBE_TEST_NTDDK_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
typedef void VOID;typedef void *PVOID;typedef int32_t NTSTATUS,LONG;typedef uint32_t ULONG;typedef uint8_t BOOLEAN,KIRQL;typedef uint64_t ULONGLONG;
typedef struct { int64_t QuadPart; } PHYSICAL_ADDRESS,LARGE_INTEGER;
typedef struct { ULONG size,offset; PVOID address; } MDL,*PMDL;
typedef void *PDEVICE_OBJECT;typedef void *PIRP;
typedef struct { PHYSICAL_ADDRESS Address;ULONG Length; } SCATTER_GATHER_ELEMENT;
typedef struct { ULONG NumberOfElements; SCATTER_GATHER_ELEMENT Elements[2]; } SCATTER_GATHER_LIST,*PSCATTER_GATHER_LIST;
struct _DMA_ADAPTER;typedef _DMA_ADAPTER *PDMA_ADAPTER;
typedef void (*DRIVER_LIST_CONTROL)(PDEVICE_OBJECT,PIRP,PSCATTER_GATHER_LIST,PVOID);
struct DMA_OPERATIONS { NTSTATUS (*GetScatterGatherList)(PDMA_ADAPTER,PDEVICE_OBJECT,PMDL,PVOID,ULONG,DRIVER_LIST_CONTROL,PVOID,BOOLEAN);void (*PutScatterGatherList)(PDMA_ADAPTER,PSCATTER_GATHER_LIST,BOOLEAN); };
struct _DMA_ADAPTER { DMA_OPERATIONS *DmaOperations; };
typedef struct { int signaled; } KEVENT;
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER (-1)
#define STATUS_INSUFFICIENT_RESOURCES (-2)
#define STATUS_DEVICE_CONFIGURATION_ERROR (-3)
#define STATUS_IO_TIMEOUT (-4)
#define STATUS_TIMEOUT 258
#define TRUE 1
#define FALSE 0
#define PASSIVE_LEVEL 0
#define DISPATCH_LEVEL 2
#define NonPagedPool 0
#define NotificationEvent 0
#define Executive 0
#define KernelMode 0
#define IO_NO_INCREMENT 0
#define NT_SUCCESS(x) ((x)>=0)
#define MmGetMdlByteOffset(m) ((m)->offset)
#define MmGetMdlByteCount(m) ((m)->size)
#define MmGetMdlVirtualAddress(m) ((m)->address)
extern int probe_allocated;
inline PVOID ExAllocatePoolWithTag(int,size_t n,ULONG){probe_allocated++;return malloc(n);}
inline void ExFreePoolWithTag(PVOID p,ULONG){probe_allocated--;free(p);}
inline LONG InterlockedDecrement(volatile LONG *p){return __sync_sub_and_fetch(p,1);}
inline void KeInitializeEvent(KEVENT *e,int,int){e->signaled=0;}
inline void KeSetEvent(KEVENT *e,int,int){e->signaled=1;}
inline int KeGetCurrentIrql(){return 0;}
inline void KeRaiseIrql(KIRQL,KIRQL *old){*old=0;}
inline void KeLowerIrql(KIRQL){}
inline NTSTATUS KeWaitForSingleObject(KEVENT *e,int,int,int,LARGE_INTEGER *){return e->signaled?STATUS_SUCCESS:STATUS_TIMEOUT;}
#endif
