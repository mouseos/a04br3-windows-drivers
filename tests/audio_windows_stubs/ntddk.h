#ifndef AUDIO_TEST_NTDDK_H
#define AUDIO_TEST_NTDDK_H
#include <stdint.h>
typedef void *PVOID; typedef uint32_t ULONG; typedef unsigned char BOOLEAN,KIRQL; typedef uintptr_t KAFFINITY; typedef int32_t NTSTATUS,LONG; typedef enum { LevelSensitive, Latched } KINTERRUPT_MODE;
#define FALSE 0
#define TRUE 1
#define NULL 0
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
inline void KeInitializeDpc(PKDPC d,DPC_FUNC f,PVOID c) { d->fn=f;d->ctx=c;d->queued=0; }
inline BOOLEAN KeInsertQueueDpc(PKDPC d,PVOID,PVOID) { if(d->queued)return FALSE;d->queued=1;return TRUE; }
inline BOOLEAN KeRemoveQueueDpc(PKDPC d) { int q=d->queued;d->queued=0;return q; }
inline void KeFlushQueuedDpcs() { }
inline NTSTATUS IoConnectInterrupt(PKINTERRUPT *p,ISR_FUNC f,PVOID c,void *,ULONG,KIRQL,KIRQL,KINTERRUPT_MODE,BOOLEAN,KAFFINITY,BOOLEAN) { test_interrupt.fn=f;test_interrupt.ctx=c;*p=&test_interrupt;return 0; }
inline void IoDisconnectInterrupt(PKINTERRUPT) { test_interrupt.fn=0; }
#endif
