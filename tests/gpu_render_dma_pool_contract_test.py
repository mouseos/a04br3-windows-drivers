#!/usr/bin/env python3
"""RT8.1 pool capacity regression; compile the actual CreateContext body."""
import pathlib,re,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
p=root/'drivers/gpu/mt8167/wddm-render'
s=(p/'kmd_objects.c').read_text()
body=s[s.index('NTSTATUS APIENTRY GeoCreateContext('):s.index('struct context_check')]
shim=r"""
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef void *HANDLE;typedef int32_t NTSTATUS;
#define APIENTRY
#define STATUS_SUCCESS 0
#define STATUS_NOT_SUPPORTED -1
#define STATUS_DEVICE_NOT_READY -2
#define STATUS_INSUFFICIENT_RESOURCES -3
#define NonPagedPool 0
#define GEO_DRAW_LINK_COMMAND_MAX 0x401a0u
#define ExAllocatePoolWithTag(a,b,c) calloc(1,b)
#define RtlZeroMemory(a,b) memset(a,0,b)
struct geo_dma_ticket {uint32_t a,b,c,d;};
struct geo_kmd_adapter {int x;};
struct geo_kmd_context;
struct geo_kmd_device {struct geo_kmd_adapter *adapter;struct geo_kmd_context *contexts;};
struct geo_kmd_context {struct geo_kmd_adapter *adapter;struct geo_kmd_device *device;HANDLE runtime;struct geo_kmd_context *next;};
typedef struct {unsigned NodeOrdinal,EngineAffinity;struct {unsigned Value;} Flags;unsigned PrivateDriverDataSize;HANDLE hContext;struct {unsigned DmaBufferSize,DmaBufferPrivateDataSize,AllocationListSize,PatchLocationListSize;} ContextInfo;} DXGKARG_CREATECONTEXT;
static int GeoSessionEnter(struct geo_kmd_adapter*a,int n){return a!=0;}
static void GeoSessionLeave(struct geo_kmd_adapter*a,int n){(void)a;(void)n;}
"""
main=r"""
int main(void){
 struct geo_kmd_adapter a={0};struct geo_kmd_device d={&a,0};DXGKARG_CREATECONTEXT q={0};
 q.EngineAffinity=1;assert(GeoCreateContext(&d,&q)==0);
 assert(q.ContextInfo.DmaBufferSize==0x401a0u);
 assert(q.ContextInfo.DmaBufferPrivateDataSize==16);
 assert(q.ContextInfo.PatchLocationListSize==2);
 unsigned requested=q.ContextInfo.AllocationListSize;
 /* Exact saved RT AddBuffer normalization, followed by Acquire equality. */
 unsigned actual=requested;if(actual && actual<4)actual=3;
 assert(requested==actual);assert(requested>=3);
 free(q.hContext);return 0;
}
"""
build=root/'build';build.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=build,prefix='dma-contract-') as td:
 c=pathlib.Path(td)/'context.c';exe=pathlib.Path(td)/'context'
 c.write_text(shim+body+main)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wno-multichar',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
clear=(p/'kmd_clear.c').read_text()
# Check the source uses lower-bound capacity guards, not exact occupancy.
for obj,field,required in [('r','AllocationListSize',1),('r','AllocationListSize',2),('p','AllocationListSize','count')]:
 suffix='u?' if isinstance(required,int) else ''
 value=str(required)
 pattern=rf'{obj}->{field}\s*(==|!=|<=|>=|<|>)\s*{value}{suffix}(?![a-zA-Z0-9_])'
 guards=re.findall(pattern,clear)
 assert guards and all(op=='<' for op in guards),(pattern,guards)
# Both returned cursors contribute to RT 8.1 submission lengths.
assert clear.count('r->pDmaBufferPrivateData=(UCHAR *)r->pDmaBufferPrivateData+sizeof(t)')==2
assert clear.count('r->pDmaBuffer=(UCHAR *)r->pDmaBuffer+sizeof(t)')==2
print('PASS: actual CreateContext matches RT pool; capacity guards accept surplus; both DMA cursors consumed')
