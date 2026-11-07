#ifndef A04_CAPTURE_GATE_TEST_PORTCLS_H
#define A04_CAPTURE_GATE_TEST_PORTCLS_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef int32_t NTSTATUS;typedef unsigned char BOOLEAN;typedef size_t SIZE_T;
#define FALSE 0
#define TRUE 1
#define APC_LEVEL 1
#define STATUS_SUCCESS ((NTSTATUS)0)
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#define STATUS_INVALID_DEVICE_STATE ((NTSTATUS)0xc0000184)
#define STATUS_INVALID_USER_BUFFER ((NTSTATUS)0xc00000e8)
#define STATUS_PARTIAL_COPY ((NTSTATUS)0x8000000d)
#define NT_SUCCESS(s) ((s)>=0)
#define MM_COPY_MEMORY_VIRTUAL 1
#define IOCTL_KS_PROPERTY 0x002f0003u
#define KSPROPERTY_RTAUDIO_HWLATENCY 3
struct GUID {uint32_t a;uint16_t b,c;unsigned char d[8];};
static const GUID KSPROPSETID_RtAudio={1,2,3,{4}};
static inline bool IsEqualGUIDAligned(const GUID&a,const GUID&b){return !memcmp(&a,&b,sizeof(a));}
struct KSPROPERTY{GUID Set;uint32_t Id,Flags;};
struct MM_COPY_ADDRESS {void *VirtualAddress;};
struct IO_STACK_LOCATION {struct {struct {uint32_t IoControlCode,InputBufferLength;void *Type3InputBuffer;} DeviceIoControl;} Parameters;};
struct IRP {IO_STACK_LOCATION stack;};typedef IRP*PIRP;typedef IO_STACK_LOCATION*PIO_STACK_LOCATION;
static inline PIO_STACK_LOCATION IoGetCurrentIrpStackLocation(PIRP p){return &p->stack;}
unsigned KeGetCurrentIrql();
NTSTATUS MmCopyMemory(void*,MM_COPY_ADDRESS,SIZE_T,unsigned,SIZE_T*);
#endif
