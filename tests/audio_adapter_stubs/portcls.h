#ifndef AUDIO_ADAPTER_TEST_PORTCLS_H
#define AUDIO_ADAPTER_TEST_PORTCLS_H
#include <stdint.h>
#include <stddef.h>
typedef unsigned char KIRQL;typedef uintptr_t KAFFINITY;typedef enum { LevelSensitive, Latched } KINTERRUPT_MODE;
typedef void VOID;typedef void *PVOID;typedef uint32_t ULONG;typedef uintptr_t ULONG_PTR;typedef int32_t NTSTATUS,LONG;typedef unsigned char BOOLEAN;typedef ULONG *PULONG;
#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER (-1)
#define STATUS_DEVICE_BUSY (-2)
#define STATUS_DEVICE_CONFIGURATION_ERROR (-3)
#define STATUS_DEVICE_NOT_READY (-4)
#define NT_SUCCESS(x) ((x)>=0)
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#define PORT_CLASS_DEVICE_EXTENSION_SIZE (64 * sizeof(ULONG_PTR))
#define IRP_MJ_PNP 0
#define IRP_MN_STOP_DEVICE 1
#define IRP_MN_SURPRISE_REMOVAL 2
#define IRP_MN_REMOVE_DEVICE 3
#define CmResourceTypeMemory 1
#define CmResourceTypeInterrupt 2
#define CmResourceShareDeviceExclusive 1
#define CM_RESOURCE_INTERRUPT_LATCHED 1
struct _DEVICE_OBJECT {void *DeviceExtension;};typedef _DEVICE_OBJECT *PDEVICE_OBJECT;
struct _IO_STACK_LOCATION {int MinorFunction;};typedef _IO_STACK_LOCATION *PIO_STACK_LOCATION;
struct _IRP { _IO_STACK_LOCATION stack; };typedef _IRP *PIRP;
inline PIO_STACK_LOCATION IoGetCurrentIrpStackLocation(PIRP p){return &p->stack;}
inline PVOID InterlockedExchangePointer(PVOID volatile *p,PVOID v){PVOID old=*p;*p=v;return old;}
struct _UNICODE_STRING{};typedef _UNICODE_STRING *PUNICODE_STRING;
struct _DRIVER_OBJECT;typedef _DRIVER_OBJECT *PDRIVER_OBJECT;
typedef NTSTATUS (*PDRIVER_DISPATCH)(PDEVICE_OBJECT,PIRP);
struct _DRIVER_OBJECT {PDRIVER_DISPATCH MajorFunction[1];};
typedef NTSTATUS DRIVER_INITIALIZE(PDRIVER_OBJECT,PUNICODE_STRING);
typedef NTSTATUS DRIVER_ADD_DEVICE(PDRIVER_OBJECT,PDEVICE_OBJECT);
struct _MDL{};typedef _MDL *PMDL;typedef int MEMORY_CACHING_TYPE;
struct KSAUDIO_POSITION{};typedef KSAUDIO_POSITION *PKSAUDIO_POSITION;typedef void *PDEVICE_DESCRIPTION;
struct PHYSICAL_ADDRESS {int64_t QuadPart;};
struct _CM_PARTIAL_RESOURCE_DESCRIPTOR {ULONG Flags;unsigned ShareDisposition;union {struct {PHYSICAL_ADDRESS Start;ULONG Length;} Memory;} u;};typedef _CM_PARTIAL_RESOURCE_DESCRIPTOR *PCM_PARTIAL_RESOURCE_DESCRIPTOR;
struct IResourceList {virtual ULONG NumberOfEntries()=0;virtual ULONG NumberOfEntriesOfType(int)=0;virtual PCM_PARTIAL_RESOURCE_DESCRIPTOR FindTranslatedEntry(int,ULONG)=0;virtual PCM_PARTIAL_RESOURCE_DESCRIPTOR FindUntranslatedEntry(int,ULONG)=0;};typedef IResourceList *PRESOURCELIST;
struct IMiniportWaveRT {virtual ULONG Release()=0;};typedef IMiniportWaveRT *PMINIPORTWAVERT;
typedef NTSTATUS (*PCPFNSTARTDEVICE)(PDEVICE_OBJECT,PIRP,PRESOURCELIST);
NTSTATUS PcInitializeAdapterDriver(PDRIVER_OBJECT,PUNICODE_STRING,DRIVER_ADD_DEVICE*);
NTSTATUS PcAddAdapterDevice(PDRIVER_OBJECT,PDEVICE_OBJECT,PCPFNSTARTDEVICE,ULONG,ULONG);
NTSTATUS PcGetPhysicalDeviceObject(PDEVICE_OBJECT,PDEVICE_OBJECT*);
NTSTATUS PcDispatchIrp(PDEVICE_OBJECT,PIRP);
#endif
