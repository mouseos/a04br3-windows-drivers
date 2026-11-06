#ifndef TEST_WIFI_IRQ_NTDDK_H
#define TEST_WIFI_IRQ_NTDDK_H
#include <stdint.h>
#include <stddef.h>
typedef void VOID;typedef void *PVOID;typedef uint8_t UCHAR,BOOLEAN,KIRQL;typedef uint16_t USHORT;typedef uint32_t ULONG;typedef int32_t LONG,NTSTATUS;typedef uintptr_t KAFFINITY;
typedef struct {int unused;} KINTERRUPT,*PKINTERRUPT;typedef struct _KDPC KDPC,*PKDPC;struct _KDPC {void (*fn)(PKDPC,PVOID,PVOID,PVOID);PVOID context;};
typedef struct {UCHAR Type,ShareDisposition;USHORT Flags;union {struct {ULONG Level,Vector;KAFFINITY Affinity;} Interrupt;} u;} CM_PARTIAL_RESOURCE_DESCRIPTOR;
#define STATUS_SUCCESS 0
#define STATUS_DEVICE_CONFIGURATION_ERROR (-1)
#define STATUS_DEVICE_NOT_READY (-2)
#define NT_SUCCESS(x) ((x)>=0)
#define PASSIVE_LEVEL 0
#define CmResourceTypeInterrupt 2
#define CmResourceShareDeviceExclusive 1
#define CM_RESOURCE_INTERRUPT_LATCHED 1
#define LevelSensitive 0
#define TRUE 1
#define FALSE 0
#define UNREFERENCED_PARAMETER(x) ((void)(x))
static inline LONG InterlockedExchange(volatile LONG *p,LONG v){return __atomic_exchange_n(p,v,__ATOMIC_SEQ_CST);}
static inline LONG InterlockedCompareExchange(volatile LONG *p,LONG v,LONG old){__atomic_compare_exchange_n(p,&old,v,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);return old;}
int KeGetCurrentIrql(void);ULONG READ_REGISTER_ULONG(volatile ULONG *);void WRITE_REGISTER_ULONG(volatile ULONG *,ULONG);
void KeInitializeDpc(PKDPC,void (*)(PKDPC,PVOID,PVOID,PVOID),PVOID);BOOLEAN KeInsertQueueDpc(PKDPC,PVOID,PVOID);BOOLEAN KeRemoveQueueDpc(PKDPC);void KeFlushQueuedDpcs(void);
NTSTATUS IoConnectInterrupt(PKINTERRUPT *,BOOLEAN (*)(PKINTERRUPT,PVOID),PVOID,void *,ULONG,KIRQL,KIRQL,int,BOOLEAN,KAFFINITY,BOOLEAN);
void IoDisconnectInterrupt(PKINTERRUPT);BOOLEAN KeSynchronizeExecution(PKINTERRUPT,BOOLEAN (*)(PVOID),PVOID);
#endif
