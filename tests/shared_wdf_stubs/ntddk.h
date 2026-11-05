#ifndef A04_TEST_NTDDK_H
#define A04_TEST_NTDDK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
typedef const wchar_t *PCWSTR;
typedef void VOID;typedef void *PVOID;typedef uint8_t UCHAR;typedef uint16_t USHORT;typedef uint32_t ULONG;typedef int32_t NTSTATUS,LONG;typedef int64_t LONGLONG;
static inline LONG InterlockedExchange(volatile LONG *p,LONG v){return __atomic_exchange_n(p,v,__ATOMIC_SEQ_CST);}
static inline LONG InterlockedCompareExchange(volatile LONG *p,LONG v,LONG old){__atomic_compare_exchange_n(p,&old,v,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);return old;}
typedef struct { int64_t QuadPart; } PHYSICAL_ADDRESS;typedef PHYSICAL_ADDRESS LARGE_INTEGER;
typedef struct {int held;} FAST_MUTEX;
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER (-1)
#define STATUS_DEVICE_NOT_READY (-2)
#define STATUS_DEVICE_BUSY (-9)
#define STATUS_INVALID_DEVICE_STATE (-3)
#define STATUS_DEVICE_CONFIGURATION_ERROR (-4)
#define STATUS_INSUFFICIENT_RESOURCES (-5)
#define NT_SUCCESS(x) ((x)>=0)
#define PASSIVE_LEVEL 0
#define APC_LEVEL 1
#define KernelMode 0
#define TRUE 1
#define FALSE 0
#define CmResourceTypeMemory 1
#define CmResourceTypeInterrupt 2
#define CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE 0
#define CmResourceShareDeviceExclusive 1
#define MmNonCached 0
#define CM_RESOURCE_MEMORY_READ_WRITE 0
typedef struct {UCHAR Type,ShareDisposition;USHORT Flags;union {struct {ULONG Length,Alignment;PHYSICAL_ADDRESS MinimumAddress,MaximumAddress;} Memory;struct {ULONG MinimumVector,MaximumVector;} Interrupt;} u;} IO_RESOURCE_DESCRIPTOR;
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#define RtlZeroMemory(p,n) memset(p,0,n)
typedef struct {uint32_t a;uint16_t b,c;uint8_t d[8];} GUID;
#define DEFINE_GUID(name,a,b,c,d,e,f,g,h,i,j,k) const GUID name={a,b,c,{d,e,f,g,h,i,j,k}}
struct _DRIVER_OBJECT{};typedef struct _DRIVER_OBJECT *PDRIVER_OBJECT;
typedef struct _UNICODE_STRING {PCWSTR value;} UNICODE_STRING,*PUNICODE_STRING;
void RtlInitUnicodeString(PUNICODE_STRING,PCWSTR);
typedef void (*PINTERFACE_REFERENCE)(PVOID);
typedef PINTERFACE_REFERENCE PINTERFACE_DEREFERENCE;
typedef struct {USHORT Size,Version;PVOID Context;PINTERFACE_REFERENCE InterfaceReference,InterfaceDereference;} INTERFACE,*PINTERFACE;
typedef struct {UCHAR Type,ShareDisposition;struct {struct {PHYSICAL_ADDRESS Start;ULONG Length;} Memory;} u;} CM_PARTIAL_RESOURCE_DESCRIPTOR,*PCM_PARTIAL_RESOURCE_DESCRIPTOR;
void ExInitializeFastMutex(FAST_MUTEX *);void ExAcquireFastMutex(FAST_MUTEX *);void ExReleaseFastMutex(FAST_MUTEX *);
PVOID MmMapIoSpace(PHYSICAL_ADDRESS,ULONG,int);void MmUnmapIoSpace(PVOID,ULONG);
ULONG READ_REGISTER_ULONG(volatile ULONG *);void WRITE_REGISTER_ULONG(volatile ULONG *,ULONG);
void KeStallExecutionProcessor(ULONG);int KeGetCurrentIrql(void);
NTSTATUS KeDelayExecutionThread(int,int,LARGE_INTEGER *);
#endif
