#!/usr/bin/env python3
"""Execute the real binding TU with strict V3 DMA ownership spies."""
import argparse,subprocess,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[2];src=r/'vendor_m1_11/render_owner/native_owner_mdl.c'
header=r'''
#ifndef TEST_NTDDK
#define TEST_NTDDK
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef uint32_t ULONG;typedef unsigned char UCHAR,BOOLEAN;typedef void*PVOID;typedef int32_t NTSTATUS;typedef unsigned KIRQL;typedef uint64_t PFN_NUMBER;typedef ULONG*PULONG;typedef struct{uint32_t unused;}DEVICE_OBJECT,*PDEVICE_OBJECT;
typedef struct{int64_t QuadPart;}PHYSICAL_ADDRESS;
typedef struct mdl{ULONG MdlFlags,bytes;void*va;PFN_NUMBER pfns[32];}MDL,*PMDL;
typedef struct{PHYSICAL_ADDRESS Address;ULONG Length;uintptr_t Reserved;}SCATTER_GATHER_ELEMENT;
typedef struct{ULONG NumberOfElements;uintptr_t Reserved;SCATTER_GATHER_ELEMENT Elements[];}SCATTER_GATHER_LIST,*PSCATTER_GATHER_LIST;
typedef struct{ULONG Version;struct{ULONG MapRegisterCount,ScatterGatherElementCount,ScatterGatherListSize;}V1;}DMA_TRANSFER_INFO;
typedef enum{KeepObject,DeallocateObject,DeallocateObjectKeepRegisters}IO_ALLOCATION_ACTION;
struct adapter;typedef struct adapter*PDMA_ADAPTER;
typedef struct {ULONG Size;NTSTATUS(*InitializeDmaTransferContext)(PDMA_ADAPTER,void*);NTSTATUS(*GetDmaTransferInfo)(PDMA_ADAPTER,PMDL,uint64_t,ULONG,BOOLEAN,DMA_TRANSFER_INFO*);NTSTATUS(*AllocateAdapterChannelEx)(PDMA_ADAPTER,PDEVICE_OBJECT,void*,ULONG,ULONG,void*,void*,void**);NTSTATUS(*MapTransferEx)(PDMA_ADAPTER,PMDL,void*,uint64_t,ULONG,ULONG*,BOOLEAN,PSCATTER_GATHER_LIST,ULONG,void*,void*);NTSTATUS(*FlushAdapterBuffersEx)(PDMA_ADAPTER,PMDL,void*,uint64_t,ULONG,BOOLEAN);void(*FreeMapRegisters)(PDMA_ADAPTER,void*,ULONG);void(*FreeAdapterObject)(PDMA_ADAPTER,IO_ALLOCATION_ACTION);}DMA_OPERATIONS,*PDMA_OPERATIONS;
struct adapter{PDMA_OPERATIONS DmaOperations;};
#define DMA_TRANSFER_CONTEXT_SIZE_V1 64
#define DMA_TRANSFER_INFO_VERSION1 1
#define DMA_SYNCHRONOUS_CALLBACK 1
#define MDL_PAGES_LOCKED 1
#define MDL_IO_SPACE 2
#define NonPagedPoolNx 0
#define NormalPagePriority 0
#define PASSIVE_LEVEL 0
#define DISPATCH_LEVEL 2
#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER ((int32_t)0xc000000d)
#define STATUS_INVALID_DEVICE_STATE ((int32_t)0xc0000184)
#define STATUS_DEVICE_CONFIGURATION_ERROR ((int32_t)0xc0000182)
#define STATUS_INSUFFICIENT_RESOURCES ((int32_t)0xc000009a)
#define STATUS_NOT_SUPPORTED ((int32_t)0xc00000bb)
#define STATUS_DEVICE_BUSY ((int32_t)0x80000011)
#define NT_SUCCESS(s) ((NTSTATUS)(s)>=0)
#define MmGetMdlByteOffset(m) 0
#define MmGetMdlByteCount(m) ((m)->bytes)
#define MmGetMdlVirtualAddress(m) ((m)->va)
#define MmGetMdlPfnArray(m) ((m)->pfns)
#define RtlZeroMemory(p,n) memset(p,0,n)
static unsigned current_irql;
static unsigned KeGetCurrentIrql(void){return current_irql;}
static void KeRaiseIrql(unsigned n,unsigned*old){*old=current_irql;current_irql=n;}
static void KeLowerIrql(unsigned old){current_irql=old;}
static void*ExAllocatePoolWithTag(unsigned pool,size_t n,unsigned tag){(void)pool;(void)tag;return calloc(1,n);}
static void ExFreePoolWithTag(void*p,unsigned tag){(void)tag;free(p);}
static void*MmGetSystemAddressForMdlSafe(PMDL m,unsigned p){(void)p;return m->va;}
static void KeFlushIoBuffers(PMDL m,BOOLEAN read,BOOLEAN dma){(void)m;(void)read;assert(dma);}
#endif
'''
(a.out/'ntddk.h').write_text(header)
code=r'''
#include <stdio.h>
#include "ntddk.h"
#include "SOURCE"
static unsigned channel,allocated,flushed,freed,requests,fail_at,flush_fail,leases,releases;
static void*contexts[64];static BOOLEAN direction;
static NTSTATUS init(PDMA_ADAPTER a,void*c){(void)a;assert(current_irql==0);for(unsigned i=0;i<requests;i++)assert(contexts[i]!=c);contexts[requests]=c;memset(c,0,64);return 0;}
static NTSTATUS info(PDMA_ADAPTER a,PMDL m,uint64_t off,ULONG n,BOOLEAN write,DMA_TRANSFER_INFO*i){(void)a;(void)m;assert(current_irql==0);assert(n<=65536&&off%65536==0&&write==direction);assert(i->Version==1);i->V1.MapRegisterCount=n/4096;i->V1.ScatterGatherListSize=sizeof(SCATTER_GATHER_LIST)+16*sizeof(SCATTER_GATHER_ELEMENT);return 0;}
static NTSTATUS alloc(PDMA_ADAPTER a,PDEVICE_OBJECT p,void*c,ULONG n,ULONG flags,void*cb,void*arg,void**base){(void)a;(void)p;(void)c;assert(!channel&&current_irql==0&&flags==DMA_SYNCHRONOUS_CALLBACK&&!cb&&!arg);requests++;if(requests==fail_at)return STATUS_INSUFFICIENT_RESOURCES;channel=1;allocated++;*base=(void*)(uintptr_t)(requests+100);assert(n<=16);return 0;}
static NTSTATUS map(PDMA_ADAPTER a,PMDL m,void*base,uint64_t off,ULONG device,ULONG*length,BOOLEAN write,PSCATTER_GATHER_LIST sg,ULONG size,void*cb,void*arg){(void)a;(void)size;assert(channel&&base&&current_irql==0&&!device&&!cb&&!arg&&write==direction);sg->NumberOfElements=*length/4096;for(ULONG i=0;i<sg->NumberOfElements;i++){sg->Elements[i].Address.QuadPart=(int64_t)m->pfns[off/4096+i]*4096;sg->Elements[i].Length=4096;}return 0;}
static void object(PDMA_ADAPTER a,IO_ALLOCATION_ACTION how){(void)a;assert(channel&&how==DeallocateObjectKeepRegisters);channel=0;}
static NTSTATUS flush(PDMA_ADAPTER a,PMDL m,void*base,uint64_t off,ULONG n,BOOLEAN write){(void)a;(void)m;(void)off;assert(!channel&&base&&n<=65536&&current_irql==0&&write==direction);if(flush_fail)return STATUS_DEVICE_BUSY;flushed++;return 0;}
static void release_registers(PDMA_ADAPTER a,void*base,ULONG n){(void)a;assert(base&&n&&current_irql==2&&flushed>freed);freed++;}
static NTSTATUS retain(void*c,PMDL m){assert(c==m);leases++;return 0;}
static void drop(void*c,PMDL m){assert(c==m);releases++;}
void a04_gpu_dma_barrier(void){}
static DMA_OPERATIONS ops={sizeof(ops),init,info,alloc,map,flush,release_registers,object};
static void reset(void){assert(!channel);allocated=flushed=freed=requests=fail_at=flush_fail=leases=releases=0;memset(contexts,0,sizeof(contexts));}
int main(void){MDL mdl={0};struct adapter a={&ops};DEVICE_OBJECT p={0};struct a04_native_mdl_binding*b=NULL;struct a04_native_mdl_lease lease={&mdl,retain,drop};unsigned tests=0;mdl.MdlFlags=MDL_PAGES_LOCKED;mdl.bytes=131072;mdl.va=aligned_alloc(4096,mdl.bytes);assert(mdl.va);for(unsigned i=0;i<32;i++)mdl.pfns[i]=0x100+i;
reset();assert(a04_native_mdl_bind(&b,&a,&p,&mdl,mdl.bytes,&lease)==0&&requests==0&&a04_native_mdl_cpu(b)==mdl.va);tests++;
assert(a04_native_mdl_sync_for_gpu(b)==0&&requests==2&&allocated==2&&!channel&&!a04_native_mdl_cpu(b));ULONG count=0;assert(a04_native_mdl_pages(b,&count)&&count==32);tests++;
assert(a04_native_mdl_mark_exposed(b)==0);assert(a04_native_mdl_sync_for_cpu(b)==STATUS_INVALID_DEVICE_STATE&&!freed);tests++;
assert(a04_native_mdl_detach_after_mmu_ack(b)==0);assert(a04_native_mdl_sync_for_cpu(b)==0&&flushed==2&&freed==2&&a04_native_mdl_cpu(b)==mdl.va&&!a04_native_mdl_pages(b,&count));tests++;
requests=0;memset(contexts,0,sizeof(contexts));assert(a04_native_mdl_sync_for_gpu(b)==0);assert(a04_native_mdl_mark_exposed(b)==0);assert(a04_native_mdl_retire_after_gpu_off(b)==0&&flushed==4&&freed==4);assert(a04_native_mdl_release_cold(&b)==0&&!b&&releases==1);tests++;
reset();fail_at=2;assert(a04_native_mdl_bind(&b,&a,&p,&mdl,mdl.bytes,&lease)==0);assert(a04_native_mdl_sync_for_gpu(b)==STATUS_INSUFFICIENT_RESOURCES&&flushed==1&&freed==1&&a04_native_mdl_cpu(b));assert(a04_native_mdl_release_cold(&b)==0&&!b);tests++;
reset();direction=TRUE;assert(a04_native_mdl_bind_input(&b,&a,&p,&mdl,mdl.bytes,&lease)==0);assert(a04_native_mdl_sync_for_gpu(b)==0);assert(a04_native_mdl_mark_exposed(b)==0);assert(a04_native_mdl_detach_after_mmu_ack(b)==0);flush_fail=1;assert(a04_native_mdl_sync_for_cpu(b)==STATUS_DEVICE_BUSY&&freed==0&&!a04_native_mdl_cpu(b));assert(a04_native_mdl_release_cold(&b)==STATUS_DEVICE_BUSY&&b&&releases==0);tests++;
printf("{\"cases_passed\":%u,\"fault_resources_retained\":true,\"device_verified\":false}\n",tests);free(mdl.va);return 0;}
'''.replace(chr(34)+'SOURCE'+chr(34),chr(34)+str(src)+chr(34))
def function(path,name):
 text=path.read_text();at=text.index(name+'(');start=text.rfind('\n',0,at)+1;first=text.index('{',at);i=first+1;depth=1
 while depth:
  depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]
extra=r'''

#define A04_GE8300_NATIVE_OWNER_MDL_TARGET 1
#define TAG 0x644d474eu
#define STATUS_IO_DEVICE_ERROR ((int32_t)0xc0000185)
#define STATUS_IO_TIMEOUT ((int32_t)0xc00000b5)
#define RGXFWIF_KCCB_CMD_MMUCACHE 102
typedef struct{uint32_t ui32Addr;}test_addr;
struct test_cmd;
struct a04_vendor_uncached {void *cpu;};
struct a04_rgx_fw_runtime {uint32_t *command_sync;test_addr command_sync_addr;struct test_cmd *kcommands;};
struct test_firmware{int hardware_owned;struct a04_rgx_fw_runtime*runtime;};
struct a04_vendor_render_cold{struct test_firmware*firmware;struct{uint32_t memory;}layout;};
struct a04_vendor_native_cold{struct a04_vendor_render_cold*render;struct a04_native_mdl_binding*registered_target,*registered_vertex;uint64_t target_offset,vertex_offset;struct{struct a04_native_mdl_binding*binding;uint64_t offset;}linear[8];ULONG linear_count;int quarantined;struct a04_vendor_uncached output[4];};
struct a04_vendor_start{struct test_firmware*memory;uint32_t command_sequence,command_busy;};
typedef struct{uint32_t ui32ReadOffset,ui32WriteOffset;}RGXFWIF_CCB_CTL;
typedef struct test_cmd{uint32_t eCmdType,eDM;union{struct{struct{uint32_t ui32Addr;}sMemoryContext;uint32_t ui32Flags;test_addr sMMUCacheSync;uint16_t ui16MMUCacheSyncUpdateValue;}sMMUCacheData;}uCmdData;}RGXFWIF_KCCB_CMD;
static int timeout_mode;static struct a04_vendor_native_cold*active;
static int a04_rgx_device_write_words(volatile void*dst,const void*src,unsigned n){memcpy((void*)dst,src,n);return 0;}
static int a04_rgx_device_read_words(void*dst,const volatile void*src,unsigned n){memcpy(dst,(const void*)src,n);return 0;}
static int read_queue(struct a04_rgx_fw_runtime*f,RGXFWIF_CCB_CTL*k){(void)f;memset(k,0,sizeof(*k));return 0;}
static void publish(struct a04_vendor_start*s,struct a04_rgx_fw_runtime*f,uint32_t next){(void)next;for(unsigned i=0;i<1024;i++)assert(((uint64_t*)active->output[2+i/512].cpu)[i%512]==0);if(!timeout_mode)*f->command_sync=s->command_sequence;}
static NTSTATUS wait_kernel(struct a04_rgx_fw_runtime*f,uint32_t next){(void)f;(void)next;return timeout_mode?STATUS_IO_TIMEOUT:0;}
static void KeStallExecutionProcessor(unsigned n){(void)n;}
static NTSTATUS a04_vendor_native_target_acquire_pt_quiescent(struct a04_vendor_native_cold*n){(void)n;assert(!"GPU reacquire not used by these CPU-handoff cases");return STATUS_NOT_SUPPORTED;}
'''
extra+='\n'+function(r/'vendor_m1_11/render_owner/windows_native_target.c','binding_set')+'\n'+function(r/'vendor_m1_11/render_owner/windows_native_target.c','a04_vendor_native_target_remove_pt')+'\n'+function(r/'vendor_m1_11/render_owner/windows_native_target.c','a04_vendor_native_target_cpu_after_mmu_ack')+'\n'+function(r/'vendor_m1_11/render_owner/windows_native_execute.c','a04_vendor_native_target_handoff')
extra+=r'''

static void test_pt_ack(PMDL mdl,PDMA_ADAPTER a,PDEVICE_OBJECT p,const struct a04_native_mdl_lease*l){
 for(timeout_mode=0;timeout_mode<=1;timeout_mode++){
  struct a04_native_mdl_binding*b=NULL;uint64_t pt[1024];uint32_t sync=0;RGXFWIF_KCCB_CMD commands[128]={0};
  struct a04_rgx_fw_runtime fw={&sync,{123},commands};struct test_firmware mem={1,&fw};struct a04_vendor_render_cold render={&mem,{0x120}};struct a04_vendor_native_cold n={0};struct a04_vendor_start start={&mem,0,1};
  reset();direction=FALSE;assert(a04_native_mdl_bind(&b,a,p,mdl,mdl->bytes,l)==0&&a04_native_mdl_sync_for_gpu(b)==0&&a04_native_mdl_mark_exposed(b)==0);
  n.render=&render;n.registered_target=b;n.output[2].cpu=pt;n.output[3].cpu=pt+512;active=&n;for(unsigned i=0;i<1024;i++)pt[i]=1;
  NTSTATUS got=a04_vendor_native_target_handoff(&n,&start,TRUE);
  if(timeout_mode){assert(got==STATUS_IO_TIMEOUT&&flushed==0&&freed==0&&!a04_native_mdl_cpu(b));assert(a04_native_mdl_release_cold(&b)==STATUS_DEVICE_BUSY);}
  else{assert(got==0&&flushed==2&&freed==2&&a04_native_mdl_cpu(b));assert(commands[0].eCmdType==RGXFWIF_KCCB_CMD_MMUCACHE&&commands[0].uCmdData.sMMUCacheData.ui32Flags==0x1f&&commands[0].uCmdData.sMMUCacheData.ui16MMUCacheSyncUpdateValue==1);assert(a04_native_mdl_retire_after_gpu_off(b)==0&&a04_native_mdl_release_cold(&b)==0);}
 }
}
'''
code=code.replace('int main(void)',extra+'\nint main(void)')
code=code.replace('printf("{', 'test_pt_ack(&mdl,&a,&p,&lease);tests+=2;printf("{')
(a.out/'test.c').write_text(code);cmd=['cc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(a.out),str(a.out/'test.c'),str(r/'dma_pages.c'),'-o',str(a.out/'test')]
v=subprocess.run(cmd,capture_output=True,text=True);(a.out/'compile.log').write_text(v.stdout+v.stderr);v.check_returncode()
# A fault intentionally retains its lease; leak detection is separate from UAF/UB.
import os
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0';v=subprocess.run([str(a.out/'test')],capture_output=True,text=True,env=env);(a.out/'test.log').write_text(v.stdout+v.stderr);v.check_returncode();print(v.stdout,end='')
