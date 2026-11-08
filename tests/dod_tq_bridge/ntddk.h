#ifndef HOST_NTDDK_H
#define HOST_NTDDK_H
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#define VOID void
#define TRUE 1
#define FALSE 0
#define PASSIVE_LEVEL 0
#define Executive 0
#define KernelMode 0
#define MAXULONG_PTR UINTPTR_MAX
#define STATUS_SUCCESS ((int32_t)0)
#define STATUS_PENDING ((int32_t)0x103)
#define STATUS_NOT_SUPPORTED ((int32_t)0xc00000bb)
#define STATUS_INVALID_PARAMETER ((int32_t)0xc000000d)
#define STATUS_DEVICE_NOT_READY ((int32_t)0xc00000a3)
#define STATUS_DEVICE_BUSY ((int32_t)0x80000011)
#define STATUS_IO_DEVICE_ERROR ((int32_t)0xc0000185)
#define RtlZeroMemory(p,n) memset(p,0,n)
#define RtlCopyMemory memcpy
#define RtlEqualMemory(a,b,n) (!memcmp(a,b,n))
typedef void *PVOID;typedef uint32_t ULONG;typedef uintptr_t ULONG_PTR;typedef uint64_t ULONGLONG;
typedef int32_t NTSTATUS;typedef unsigned char BOOLEAN;typedef int32_t LONG;
typedef struct {uint32_t a;uint16_t b,c;unsigned char d[8];} GUID;
typedef pthread_mutex_t KMUTEX;
typedef struct {uint16_t Size,Version;PVOID Context;void (*InterfaceReference)(PVOID);void (*InterfaceDereference)(PVOID);} INTERFACE,*PINTERFACE;
typedef struct {const GUID *InterfaceType;uint16_t Size,Version;PINTERFACE Interface;PVOID InterfaceSpecificData;} QUERY_INTERFACE,*PQUERY_INTERFACE;
extern unsigned host_irql,host_apcs;
static inline unsigned KeGetCurrentIrql(void){return host_irql;}
static inline BOOLEAN KeAreAllApcsDisabled(void){return host_apcs;}
static inline void KeInitializeMutex(KMUTEX *m,unsigned n){(void)n;pthread_mutex_init(m,0);}
static inline int KeWaitForSingleObject(KMUTEX *m,int a,int b,int c,void *d){(void)a;(void)b;(void)c;(void)d;return pthread_mutex_lock(m);}
static inline void KeReleaseMutex(KMUTEX *m,int b){(void)b;pthread_mutex_unlock(m);}
static inline LONG InterlockedIncrement(volatile LONG *p){return __atomic_add_fetch(p,1,__ATOMIC_SEQ_CST);}
static inline LONG InterlockedDecrement(volatile LONG *p){return __atomic_sub_fetch(p,1,__ATOMIC_SEQ_CST);}
static inline LONG InterlockedCompareExchange(volatile LONG *p,LONG exchange,LONG compare){__atomic_compare_exchange_n(p,&compare,exchange,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);return compare;}
#endif
