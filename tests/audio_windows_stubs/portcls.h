#ifndef AUDIO_TEST_PORTCLS_H
#define AUDIO_TEST_PORTCLS_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef void VOID; typedef uint32_t ULONG; typedef int32_t LONG; typedef uint8_t BOOLEAN; typedef void *PVOID; typedef int32_t NTSTATUS;
#define PAGE_SIZE 4096u
#define FALSE 0
#define TRUE 1
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER (-1)
#define STATUS_IO_TIMEOUT (-7)
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
#ifdef A04_TEST_MINIPORT
#include <stdlib.h>
typedef ULONG *PULONG;
typedef struct { unsigned v; } GUID;
inline bool IsEqualGUIDAligned(const GUID &a,const GUID &b) { return a.v==b.v; }
typedef const GUID &REFIID;
extern const GUID IID_IUnknown,IID_IMiniport,IID_IMiniportWaveRT,IID_IMiniportWaveRTStream;
extern const GUID KSDATAFORMAT_TYPE_AUDIO,KSDATAFORMAT_SUBTYPE_PCM,KSDATAFORMAT_SPECIFIER_WAVEFORMATEX,KSCATEGORY_AUDIO;
#define STATICGUIDOF(x) x
#define STDMETHODIMP_(x) x
#define STATUS_NOINTERFACE (-10)
#define STATUS_NO_MATCH (-11)
#define STATUS_NOT_IMPLEMENTED (-12)
#define STATUS_NOT_SUPPORTED (-13)
#define STATUS_DEVICE_NOT_READY (-14)
#define STATUS_BUFFER_OVERFLOW (-15)
#define STATUS_BUFFER_TOO_SMALL (-16)
#define NT_SUCCESS(x) ((x)>=0)
#define WAVE_FORMAT_PCM 1
#define KSPIN_DATAFLOW_IN 1
#define KSPIN_DATAFLOW_OUT 2
#define KSPIN_COMMUNICATION_SINK 1
#define KSPIN_COMMUNICATION_NONE 0
#define PCFILTER_NODE 0xffffffffu
#define NonPagedPool 0
inline void *ExAllocatePoolWithTag(int,size_t n,ULONG) {return malloc(n);}
inline void ExFreePoolWithTag(void *p,ULONG) {free(p);}
inline LONG InterlockedIncrement(volatile LONG *p) { return ++*p; }
inline LONG InterlockedDecrement(volatile LONG *p) { return --*p; }
inline LONG InterlockedCompareExchange(volatile LONG *p,LONG v,LONG c) { LONG old=*p;if(old==c)*p=v;return old; }
inline LONG InterlockedExchange(volatile LONG *p,LONG v) { LONG old=*p;*p=v;return old; }
typedef struct { ULONG FormatSize,Flags,SampleSize,Reserved; GUID MajorFormat,SubFormat,Specifier; } KSDATAFORMAT,*PKSDATAFORMAT,*PKSDATARANGE;
typedef struct { unsigned short wFormatTag,nChannels; ULONG nSamplesPerSec,nAvgBytesPerSec; unsigned short nBlockAlign,wBitsPerSample,cbSize; } WAVEFORMATEX;
typedef struct {KSDATAFORMAT DataFormat;WAVEFORMATEX WaveFormatEx;} KSDATAFORMAT_WAVEFORMATEX,*PKSDATAFORMAT_WAVEFORMATEX;
typedef struct {KSDATAFORMAT DataRange; ULONG MaximumChannels,MinimumBitsPerSample,MaximumBitsPerSample,MinimumSampleFrequency,MaximumSampleFrequency;} KSDATARANGE_AUDIO;
struct TestPin { ULONG a; const void *b; ULONG c; const void*d; ULONG e; PKSDATARANGE *ranges; int flow,communication; const GUID *category,*name; ULONG reserved; };
struct PCPIN_DESCRIPTOR { ULONG a,b,c; const void *automation; TestPin pin; };
struct PCNODE_DESCRIPTOR { int dummy; };
struct PCCONNECTION_DESCRIPTOR { ULONG a,b,c,d; };
struct PCFILTER_DESCRIPTOR {ULONG version;void *automation;ULONG pinSize,pinCount;PCPIN_DESCRIPTOR *pins;ULONG nodeSize,nodeCount;PCNODE_DESCRIPTOR *nodes;ULONG connectionCount;PCCONNECTION_DESCRIPTOR *connections;ULONG categories;void *category;};
typedef PCFILTER_DESCRIPTOR *PPCFILTER_DESCRIPTOR;
typedef struct {uint64_t PlayOffset,WriteOffset;} KSAUDIO_POSITION,*PKSAUDIO_POSITION;
typedef struct {ULONG a,b,c;} KSRTAUDIO_HWLATENCY,KSRTAUDIO_HWREGISTER;
typedef enum {KSSTATE_STOP,KSSTATE_ACQUIRE,KSSTATE_PAUSE,KSSTATE_RUN} KSSTATE;
typedef int MEMORY_CACHING_TYPE;
typedef void *PUNKNOWN,*PRESOURCELIST,*PPORTWAVERT,*PDEVICE_DESCRIPTION;
struct IMiniportWaveRTStream {
 virtual NTSTATUS QueryInterface(REFIID,PVOID*)=0; virtual ULONG AddRef()=0; virtual ULONG Release()=0;
 virtual NTSTATUS SetFormat(PKSDATAFORMAT)=0; virtual NTSTATUS SetState(KSSTATE)=0; virtual NTSTATUS GetPosition(PKSAUDIO_POSITION)=0;
 virtual NTSTATUS AllocateAudioBuffer(ULONG,PMDL*,PULONG,PULONG,MEMORY_CACHING_TYPE*)=0; virtual void FreeAudioBuffer(PMDL,ULONG)=0;
 virtual void GetHWLatency(KSRTAUDIO_HWLATENCY*)=0; virtual NTSTATUS GetPositionRegister(KSRTAUDIO_HWREGISTER*)=0; virtual NTSTATUS GetClockRegister(KSRTAUDIO_HWREGISTER*)=0;
};
typedef IMiniportWaveRTStream *PMINIPORTWAVERTSTREAM;
struct IMiniportWaveRT {
 virtual NTSTATUS QueryInterface(REFIID,PVOID*)=0;virtual ULONG AddRef()=0;virtual ULONG Release()=0;
 virtual NTSTATUS GetDescription(PPCFILTER_DESCRIPTOR*)=0;virtual NTSTATUS DataRangeIntersection(ULONG,PKSDATARANGE,PKSDATARANGE,ULONG,PVOID,PULONG)=0;
 virtual NTSTATUS Init(PUNKNOWN,PRESOURCELIST,PPORTWAVERT)=0;virtual NTSTATUS NewStream(PMINIPORTWAVERTSTREAM*,PPORTWAVERTSTREAM,ULONG,BOOLEAN,PKSDATAFORMAT)=0;virtual NTSTATUS GetDeviceDescription(PDEVICE_DESCRIPTION)=0;
};
typedef IMiniportWaveRT *PMINIPORTWAVERT;
#endif
#endif
