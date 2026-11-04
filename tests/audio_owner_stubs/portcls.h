#ifndef AUDIO_OWNER_TEST_PORTCLS_H
#define AUDIO_OWNER_TEST_PORTCLS_H
#include "../audio_windows_stubs/ntddk.h"
typedef int MEMORY_CACHING_TYPE;
enum { MmNonCached };
typedef struct { uint64_t PlayOffset,WriteOffset; } KSAUDIO_POSITION,*PKSAUDIO_POSITION;
typedef DEVICE_DESCRIPTION *PDEVICE_DESCRIPTION;
#define CmResourceTypeInterrupt 2
#define CmResourceShareDeviceExclusive 1
#define CM_RESOURCE_INTERRUPT_LATCHED 1
#define CM_RESOURCE_INTERRUPT_MESSAGE 2
#define HIGH_LEVEL 31
struct _CM_PARTIAL_RESOURCE_DESCRIPTOR {ULONG Flags; unsigned ShareDisposition;struct { struct {ULONG Vector,Level;KAFFINITY Affinity;unsigned Group;} Interrupt;} u;};
typedef _CM_PARTIAL_RESOURCE_DESCRIPTOR *PCM_PARTIAL_RESOURCE_DESCRIPTOR;
struct IResourceList {virtual ULONG NumberOfEntriesOfType(int)=0;virtual PCM_PARTIAL_RESOURCE_DESCRIPTOR FindTranslatedEntry(int,ULONG)=0;};typedef IResourceList *PRESOURCELIST;
#endif
