#ifndef AUDIO_TEST_PORTCLS_H
#define AUDIO_TEST_PORTCLS_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint32_t ULONG; typedef uint8_t BOOLEAN; typedef void *PVOID; typedef int32_t NTSTATUS;
#define PAGE_SIZE 4096u
#define FALSE 0
#define TRUE 1
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER (-1)
#define STATUS_INSUFFICIENT_RESOURCES (-2)
#define STATUS_DEVICE_CONFIGURATION_ERROR (-3)
#define STATUS_DEVICE_BUSY (-4)
#define RtlZeroMemory(p,n) memset(p,0,n)
typedef uint64_t ULONGLONG;
typedef struct { int64_t QuadPart; } PHYSICAL_ADDRESS;
typedef struct _MDL { ULONG offset, bytes; } MDL, *PMDL;
#define MmGetMdlByteOffset(p) ((p)->offset)
#define MmGetMdlByteCount(p) ((p)->bytes)
enum { MmNonCached };
struct IPortWaveRTStream {
 virtual PMDL AllocateContiguousPagesForMdl(PHYSICAL_ADDRESS,PHYSICAL_ADDRESS,size_t)=0;
 virtual ULONG GetPhysicalPagesCount(PMDL)=0;
 virtual PHYSICAL_ADDRESS GetPhysicalPageAddress(PMDL,ULONG)=0;
 virtual PVOID MapAllocatedPages(PMDL,int)=0;
 virtual void UnmapAllocatedPages(PVOID,PMDL)=0;
 virtual void FreePagesFromMdl(PMDL)=0;
 virtual ULONG AddRef()=0;
 virtual ULONG Release()=0;
};
typedef IPortWaveRTStream *PPORTWAVERTSTREAM;

#ifdef A04_TEST_REGISTRATION
typedef void *PDEVICE_OBJECT; typedef void *PIRP; typedef void *PRESOURCELIST;
typedef wchar_t WCHAR; typedef const void *REFCLSID;
#define NT_SUCCESS(s) ((s) >= 0)
struct IUnknown { virtual ~IUnknown() {} };
typedef IUnknown *PUNKNOWN;
struct IMiniportWaveRT : IUnknown {};
typedef IMiniportWaveRT *PMINIPORTWAVERT;
struct IPort : IUnknown {
 virtual NTSTATUS Init(PDEVICE_OBJECT,PIRP,PUNKNOWN,PUNKNOWN,PRESOURCELIST)=0;
 virtual ULONG Release()=0;
};
typedef IPort *PPORT;
extern const int CLSID_PortWaveRT;
NTSTATUS PcNewPort(PPORT *, const int &);
NTSTATUS PcRegisterSubdevice(PDEVICE_OBJECT,WCHAR *,PUNKNOWN);
#endif
#endif
