#ifndef AUDIO_QUERY_NTDDK_H
#define AUDIO_QUERY_NTDDK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint32_t *PULONG;
#define STATUS_DEVICE_NOT_READY (-14)
typedef void VOID;typedef void *PVOID;typedef uint16_t USHORT;typedef uint32_t ULONG;typedef int32_t NTSTATUS;typedef unsigned char BOOLEAN;
typedef struct {uint32_t a;uint16_t b,c;uint8_t d[8];} GUID;
typedef void (*PINTERFACE_REFERENCE)(PVOID);typedef struct {USHORT Size,Version;PVOID Context;PINTERFACE_REFERENCE InterfaceReference,InterfaceDereference;} INTERFACE,*PINTERFACE;
typedef struct _DEVICE_OBJECT {int StackSize;} DEVICE_OBJECT,*PDEVICE_OBJECT;
typedef struct {int signaled;} KEVENT,*PKEVENT;
struct _IRP;typedef struct _IRP *PIRP;
typedef struct {int MajorFunction,MinorFunction;struct {struct {const GUID *InterfaceType;USHORT Size,Version;PINTERFACE Interface;PVOID InterfaceSpecificData;} QueryInterface;} Parameters;} IO_STACK_LOCATION,*PIO_STACK_LOCATION;
typedef NTSTATUS (*COMPLETE)(PDEVICE_OBJECT,PIRP,PVOID);
struct _IRP {struct {NTSTATUS Status;} IoStatus;IO_STACK_LOCATION stack;COMPLETE complete;PVOID context;};
#define IRP_MJ_PNP 1
#define IRP_MN_QUERY_INTERFACE 2
#define STATUS_INVALID_PARAMETER (-1)
#define STATUS_INSUFFICIENT_RESOURCES (-2)
#define STATUS_NOT_SUPPORTED (-3)
#define STATUS_MORE_PROCESSING_REQUIRED (-4)
#define STATUS_PENDING 0x103
#define STATUS_SUCCESS 0
#define TRUE 1
#define FALSE 0
#define PASSIVE_LEVEL 0
#define IO_NO_INCREMENT 0
#define NotificationEvent 0
#define Executive 0
#define KernelMode 0
#define NT_SUCCESS(x) ((x)>=0)
#define RtlZeroMemory(p,n) memset(p,0,n)
#define UNREFERENCED_PARAMETER(x) ((void)(x))
int KeGetCurrentIrql(void);void KeInitializeEvent(PKEVENT,int,BOOLEAN);void KeSetEvent(PKEVENT,int,BOOLEAN);void KeWaitForSingleObject(PKEVENT,int,int,BOOLEAN,PVOID);
PIRP IoAllocateIrp(int,BOOLEAN);void IoFreeIrp(PIRP);PIO_STACK_LOCATION IoGetNextIrpStackLocation(PIRP);void IoSetCompletionRoutine(PIRP,COMPLETE,PVOID,BOOLEAN,BOOLEAN,BOOLEAN);NTSTATUS IoCallDriver(PDEVICE_OBJECT,PIRP);
#endif
