#ifndef AUDIO_OWNER_TEST_PORTCLS_H
#define AUDIO_OWNER_TEST_PORTCLS_H
#include "../audio_windows_stubs/ntddk.h"
typedef int MEMORY_CACHING_TYPE;
enum { MmNonCached, MmCached, MmWriteCombined };
typedef struct { uint64_t PlayOffset,WriteOffset; } KSAUDIO_POSITION,*PKSAUDIO_POSITION;
typedef DEVICE_DESCRIPTION *PDEVICE_DESCRIPTION;
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
#define STATUS_BUFFER_TOO_SMALL (-16)
enum KSSTATE {KSSTATE_STOP,KSSTATE_ACQUIRE,KSSTATE_PAUSE,KSSTATE_RUN};
struct IPortWaveRTStream {
 virtual PMDL AllocateContiguousPagesForMdl(PHYSICAL_ADDRESS,PHYSICAL_ADDRESS,size_t)=0;
 virtual ULONG GetPhysicalPagesCount(PMDL)=0;
 virtual PHYSICAL_ADDRESS GetPhysicalPageAddress(PMDL,ULONG)=0;
 virtual PVOID MapAllocatedPages(PMDL,int)=0;
 virtual void UnmapAllocatedPages(PVOID,PMDL)=0;
 virtual void FreePagesFromMdl(PMDL)=0;
 virtual ULONG AddRef()=0;virtual ULONG Release()=0;
};
typedef IPortWaveRTStream *PPORTWAVERTSTREAM;
#endif
#define CmResourceTypeInterrupt 2
#define CmResourceTypeMemory 3
#define CmResourceShareDeviceExclusive 1
#define CM_RESOURCE_INTERRUPT_LATCHED 1
#define CM_RESOURCE_INTERRUPT_MESSAGE 2
#define HIGH_LEVEL 31
struct _CM_PARTIAL_RESOURCE_DESCRIPTOR {ULONG Flags; unsigned ShareDisposition;struct { struct {ULONG Vector,Level;KAFFINITY Affinity;unsigned Group;} Interrupt;struct {PHYSICAL_ADDRESS Start;ULONG Length;} Memory;} u;};
typedef _CM_PARTIAL_RESOURCE_DESCRIPTOR *PCM_PARTIAL_RESOURCE_DESCRIPTOR;
struct IResourceList {virtual ULONG NumberOfEntriesOfType(int)=0;virtual PCM_PARTIAL_RESOURCE_DESCRIPTOR FindTranslatedEntry(int,ULONG)=0;virtual PCM_PARTIAL_RESOURCE_DESCRIPTOR FindUntranslatedEntry(int,ULONG)=0;};typedef IResourceList *PRESOURCELIST;
#if defined(A04_AUDIO_MAP_AFE) && A04_AUDIO_MAP_AFE
PVOID MmMapIoSpace(PHYSICAL_ADDRESS,ULONG,MEMORY_CACHING_TYPE);
void MmUnmapIoSpace(PVOID,ULONG);
#endif
#endif
