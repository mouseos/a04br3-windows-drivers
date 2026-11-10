/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "clear_engine.h"
typedef void *HANDLE;typedef uint32_t UINT,ULONG,D3DKMT_HANDLE;typedef int32_t NTSTATUS,HRESULT;typedef int BOOLEAN;
#define APIENTRY
#define TRUE 1
#define STATUS_DEVICE_BUSY GEO_BUSY
#define STATUS_SUCCESS GEO_OK
#define STATUS_INVALID_PARAMETER GEO_INVALID
#define STATUS_NOT_SUPPORTED GEO_UNSUPPORTED
#define STATUS_DEVICE_NOT_READY GEO_NOT_READY
#define STATUS_INSUFFICIENT_RESOURCES ((int32_t)0xc000009a)
#define STATUS_UNSUCCESSFUL ((int32_t)0xc0000001)
#define E_INVALIDARG GEO_INVALID
#define E_OUTOFMEMORY STATUS_INSUFFICIENT_RESOURCES
#define E_UNEXPECTED STATUS_UNSUCCESSFUL
#define S_OK 0
#define FAILED(x) ((int32_t)(x)<0)
#define PASSIVE_LEVEL 0
#define NonPagedPool 0
#define HEAP_ZERO_MEMORY 8
#define PAGE_SIZE 4096
#define ROUND_TO_PAGES(x) (((x)+4095u)&~4095u)
#define D3DDDI_ALLOCATIONPRIORITY_NORMAL 0
#define DXGK_HANDLE_ALLOCATION 0
#define RtlCopyMemory memcpy
#define RtlZeroMemory(p,n) memset(p,0,n)
static int fail_heap;
static void *heap_alloc(size_t n){if(fail_heap){fail_heap=0;return NULL;}return calloc(1,n);}
#define ExAllocatePoolWithTag(pool,n,tag) heap_alloc(n)
#define ExFreePoolWithTag(p,tag) free(p)
#define GetProcessHeap() NULL
#define HeapAlloc(heap,flags,n) heap_alloc(n)
#define HeapFree(heap,flags,p) free(p)
#define KeGetCurrentIrql() PASSIVE_LEVEL
struct allocation_info {D3DKMT_HANDLE hAllocation;void *pPrivateDriverData;UINT PrivateDriverDataSize;};
typedef struct allocation_info D3DDDI_ALLOCATIONINFO,D3DDDI_OPENALLOCATIONINFO;
typedef struct {const void *pPrivateDriverData;UINT PrivateDriverDataSize;HANDLE hResource;D3DKMT_HANDLE hKMResource;UINT NumAllocations;D3DDDI_ALLOCATIONINFO *pAllocationInfo;} D3DDDICB_ALLOCATE;
struct geo_umd_resource;
struct geo_umd_device {HANDLE runtime;struct {HRESULT (*pfnAllocateCb)(HANDLE,D3DDDICB_ALLOCATE *);} cb;struct geo_umd_resource *resources;};
struct geo_umd_resource {struct geo_umd_resource *next;struct geo_umd_device *device;HANDLE runtime;struct geo_surface_desc desc;UINT logical_width,logical_height,primary;D3DKMT_HANDLE allocation;};
typedef struct {void *pPrivateDriverData;UINT PrivateDriverDataSize;HANDLE hAllocation;UINT Alignment,Size,PitchAlignedSize;union {UINT Value;struct {UINT SegmentId0;};} PreferredSegment;UINT SupportedReadSegmentSet,SupportedWriteSegmentSet,EvictionSegmentSet;struct {UINT Value;} HintedBank;UINT MaximumRenamingListLength;void *pAllocationUsageHint;struct {UINT Value,CpuVisible,Cached,SynchronousPaging;} Flags;UINT AllocationPriority;} DXGK_ALLOCATIONINFO;
typedef struct {const void *pPrivateDriverData;UINT PrivateDriverDataSize,NumAllocations;DXGK_ALLOCATIONINFO *pAllocationInfo;HANDLE hResource;struct {UINT Resource;} Flags;} DXGKARG_CREATEALLOCATION;
typedef struct {D3DKMT_HANDLE hAllocation;void *pPrivateDriverData;UINT PrivateDriverDataSize;HANDLE hDeviceSpecificAllocation;} DXGK_OPENALLOCATIONINFO;
typedef struct {UINT NumAllocations;DXGK_OPENALLOCATIONINFO *pOpenAllocation;void *pPrivateDriverData;UINT PrivateDriverSize,SubresourceIndex;struct {UINT Value,ReadOnly;} Flags;} DXGKARG_OPENALLOCATION;
typedef struct {UINT NumAllocations;HANDLE *pOpenHandleList;} DXGKARG_CLOSEALLOCATION;
typedef struct {UINT hObject,Type;} DXGKARGCB_GETHANDLEDATA;
struct geo_kmd_shared_resource {struct geo_kmd_shared_resource *next;struct geo_allocation *allocation;};
struct geo_kmd_adapter {struct geo_kmd_shared_resource *shared_resources;struct geo_engine engine;void *display;UINT aperture_ready;struct {uint64_t commit_limit;} aperture;void *(*get_handle_data)(DXGKARGCB_GETHANDLEDATA *);BOOLEAN (*synchronize)(void *,BOOLEAN (*)(void *),void *);void *sync_context;};
struct geo_open_allocation {struct geo_open_allocation *next;struct geo_allocation *allocation;UINT readonly;};
struct geo_kmd_device {struct geo_kmd_adapter *adapter;struct geo_open_allocation *opens;};
static int GeoSessionEnter(struct geo_kmd_adapter *a,int mode){(void)mode;return a!=NULL;}
static void GeoSessionLeave(struct geo_kmd_adapter *a,int mode){(void)a;(void)mode;}

typedef struct {UINT NumAllocations;HANDLE *pAllocationList;HANDLE hResource;struct {UINT DestroyResource;} Flags;} DXGKARG_DESTROYALLOCATION;
static NTSTATUS GeoDisplay356RetireAllocation(struct geo_kmd_adapter *a,struct geo_allocation *b){(void)a;(void)b;return GEO_OK;}
static void GeoEndVidMmMap(struct geo_kmd_adapter *a,struct geo_allocation *b){(void)a;(void)b;}
