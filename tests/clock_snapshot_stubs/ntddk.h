/* Host-test API model only; not a WDK header or a kernel execution claim. */
#ifndef CLOCK_TEST_NTDDK_H
#define CLOCK_TEST_NTDDK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef void *PVOID;typedef uint32_t ULONG;typedef uint8_t UCHAR;
typedef void VOID;
typedef int32_t LONG;typedef int32_t NTSTATUS;
typedef struct {uint32_t Data1;uint16_t Data2,Data3;uint8_t Data4[8];} GUID;
typedef struct {uint16_t Size,Version;PVOID Context;
 void (*InterfaceReference)(PVOID);void (*InterfaceDereference)(PVOID);} INTERFACE,*PINTERFACE;
#define PASSIVE_LEVEL 0
#define APC_LEVEL 1
#define STATUS_SUCCESS ((NTSTATUS)0)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_INVALID_DEVICE_STATE ((NTSTATUS)0xc0000184)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define STATUS_DEVICE_HARDWARE_ERROR ((NTSTATUS)0xc0000185)
#define STATUS_REVISION_MISMATCH ((NTSTATUS)0xc0000059)
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#define NT_SUCCESS(s) ((NTSTATUS)(s)>=0)
#define RtlZeroMemory(p,n) memset(p,0,n)
#define UNREFERENCED_PARAMETER(x) ((void)(x))
int KeGetCurrentIrql(void);
uint64_t KeQueryInterruptTime(void);
LONG InterlockedCompareExchange(volatile LONG *,LONG,LONG);
int ExAcquireRundownProtection(int *);
void ExReleaseRundownProtection(int *);
void ExAcquireFastMutex(int *);
void ExReleaseFastMutex(int *);
ULONG TestReadRegister(volatile ULONG *);
#define READ_REGISTER_ULONG(p) TestReadRegister(p)
#endif
