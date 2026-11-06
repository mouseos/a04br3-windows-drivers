/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/windows_shared_scanout.h"
#include "../drivers/gpu/mt8167/gpu_mmu.h"
#include "../drivers/gpu/mt8167/vendor_m1_11/windows_shared_tables.h"
static struct fake_mdl mdl;static SCATTER_GATHER_LIST gpu_list,display_list;
static PDMA_ADAPTER gpu_id,display_id;static int gpu_put,display_put,freed,fragment,duplicate;
static unsigned table_allocs,table_frees;static int table_alias;
static int proof_status,proof_calls;
static NTSTATUS proof(void *p){assert(p==(void*)3);proof_calls++;return proof_status;}
void a04_gpu_dma_barrier(void){}
NTSTATUS a04_vendor_uncached_allocate(struct a04_vendor_uncached *b,PDMA_ADAPTER a,ULONG bytes)
{
    assert(a==gpu_id && bytes==4096);b->cpu=aligned_alloc(4096,4096);
    if(!b->cpu)return STATUS_INSUFFICIENT_RESOURCES;
    b->adapter=a;b->bytes=bytes;b->logical.QuadPart=table_alias?0x41000000:0x60000000u+table_allocs*4096u;
    table_allocs++;return STATUS_SUCCESS;
}
void a04_vendor_uncached_free(struct a04_vendor_uncached *b)
{if(b->cpu){free(b->cpu);table_frees++;memset(b,0,sizeof(*b));}}
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
void KeInitializeEvent(KEVENT *e,int t,BOOLEAN b){(void)t;(void)b;e->signaled=0;}
void KeSetEvent(KEVENT *e,int p,BOOLEAN b){(void)p;(void)b;e->signaled=1;}
NTSTATUS KeWaitForSingleObject(void *p,int a,int b,BOOLEAN c,void *d)
{(void)a;(void)b;(void)c;(void)d;assert(((KEVENT*)p)->signaled);return STATUS_SUCCESS;}
void *ExAllocatePoolWithTag(int p,size_t n,unsigned tag){(void)p;(void)tag;return malloc(n);}
void ExFreePoolWithTag(void *p,unsigned tag){(void)tag;free(p);}
void ExFreePool(void *p){assert(p==&mdl);}
PMDL MmAllocatePagesForMdlEx(PHYSICAL_ADDRESS low,PHYSICAL_ADDRESS high,PHYSICAL_ADDRESS skip,SIZE_T bytes,int cache,ULONG flags)
{assert(low.QuadPart==0 && high.QuadPart==0xffffffffLL && !skip.QuadPart && bytes==4096000 && cache==MmNonCached && flags==MM_ALLOCATE_FULLY_REQUIRED);memset(&mdl,0,sizeof(mdl));mdl.bytes=(ULONG)bytes;return &mdl;}
void MmFreePagesFromMdl(PMDL m){assert(m==&mdl);freed++;}
static NTSTATUS get(PDMA_ADAPTER a,PDEVICE_OBJECT d,PMDL m,void *v,ULONG n,PDRIVER_LIST_CONTROL cb,void *ctx,BOOLEAN write)
{
    SCATTER_GATHER_LIST *list; (void)d;(void)v;assert(m==&mdl && n==4096000);
    if(a==gpu_id){assert(!write);list=&gpu_list;}else{assert(a==display_id && write);list=&display_list;}
    memset(list,0,sizeof(*list));list->NumberOfElements=2;
    list->Elements[0].Address.QuadPart=a==gpu_id?0x41000000:0x51000000;
    list->Elements[0].Length=4096;
    list->Elements[1].Address.QuadPart=list->Elements[0].Address.QuadPart+4096;
    list->Elements[1].Length=4096000-4096;
    if(a==display_id && fragment)list->Elements[1].Address.QuadPart+=4096;
    if(a==gpu_id && duplicate)list->Elements[1].Address.QuadPart=list->Elements[0].Address.QuadPart;
    cb(d,0,list,ctx);return STATUS_SUCCESS;
}
static void put(PDMA_ADAPTER a,PSCATTER_GATHER_LIST list,BOOLEAN write)
{if(a==gpu_id){assert(list==&gpu_list && !write);gpu_put++;}else{assert(a==display_id && list==&display_list && write);display_put++;}}
int main(void)
{
    DMA_OPERATIONS ops={0};struct fake_adapter ga={&ops},da={&ops};struct a04_shared_scanout s={0};
    ops.GetScatterGatherList=get;ops.PutScatterGatherList=put;gpu_id=&ga;display_id=&da;
    assert(NT_SUCCESS(a04_shared_scanout_prepare(&s,&ga,(void*)1,&da,(void*)2)));
    assert(s.mdl==&mdl && s.gpu_pages[0]==0x41000000 && s.gpu_pages[999]==0x413e7000 && s.display_dma==0x51000000);
    {
      uint32_t pc[1024]={0};uint64_t pd[512]={0},pt0[512]={0},pt1[512]={0};
      assert(a04_shared_scanout_build_tables(&s,pc,0x41000000,pd,0x60001000,pt0,0x60002000,pt1,0x60003000)==STATUS_DEVICE_CONFIGURATION_ERROR);
      assert(!pc[512] && !pd[0] && !pt0[0]);
      assert(NT_SUCCESS(a04_shared_scanout_build_tables(&s,pc,0x60000000,pd,0x60001000,pt0,0x60002000,pt1,0x60003000)));
      assert((pt0[0]&~4095ULL)==0x41000000 && (pt1[487]&~4095ULL)==0x413e7000);
      assert((pt0[0]&A04_GPU_MMU_SLC_BYPASS)!=0);
    }
    {
      struct a04_vendor_m1_11_memory fw={0};struct a04_vendor_render_cold render={0};
      struct a04_vendor_copy_scratch copy={0};struct a04_shared_tables tables={0};
      fw.adapter=&ga;render.firmware=&fw;render.copy_owner=&copy;copy.render=&render;
      fw.buffers[0].dma=0x41000000;fw.buffers[0].bytes=4096;
      assert(a04_shared_tables_stage(&tables,&s,&render,&copy)==STATUS_DEVICE_CONFIGURATION_ERROR);
      assert(tables.quarantined && !table_allocs);
      memset(&tables,0,sizeof(tables));fw.buffers[0].bytes=0;
      assert(NT_SUCCESS(a04_shared_tables_stage(&tables,&s,&render,&copy)) && tables.ready);
      assert((((uint64_t *)tables.table[2].cpu)[0]&~4095ULL)==0x41000000);
      assert((((uint64_t *)tables.table[3].cpu)[487]&~4095ULL)==0x413e7000);
      assert(NT_SUCCESS(a04_shared_tables_release(&tables)) && table_frees==4);
      table_alias=1;
      assert(a04_shared_tables_stage(&tables,&s,&render,&copy)==STATUS_DEVICE_CONFIGURATION_ERROR);
      assert(tables.quarantined && a04_shared_tables_release(&tables)==STATUS_DEVICE_BUSY && table_frees==4);
      /* Test teardown only: production deliberately retains aliases. */
      a04_vendor_uncached_free(&tables.table[0]);
      table_alias=0;
    }
    s.exposed=1;assert(a04_shared_scanout_release(&s)==STATUS_DEVICE_BUSY && !gpu_put && !display_put && !freed);
    proof_status=STATUS_DEVICE_BUSY;
    assert(a04_shared_scanout_handoff_display(&s,(void*)2,proof,(void*)3)==STATUS_DEVICE_BUSY);
    assert(proof_calls==1 && s.gpu_sg && s.display_sg && !gpu_put && !display_put);
    proof_status=STATUS_SUCCESS;
    assert(NT_SUCCESS(a04_shared_scanout_handoff_display(&s,(void*)2,proof,(void*)3)));
    assert(!s.gpu_sg && s.display_sg && s.display_dma==0x51000000 && gpu_put==1 && display_put==1);
    assert(a04_shared_scanout_handoff_display(&s,(void*)2,proof,(void*)3)==STATUS_INVALID_DEVICE_STATE);
    s.exposed=0;assert(NT_SUCCESS(a04_shared_scanout_release(&s)) && gpu_put==1 && display_put==2 && freed==1);
    fragment=1;assert(a04_shared_scanout_prepare(&s,&ga,(void*)1,&da,(void*)2)==STATUS_DEVICE_CONFIGURATION_ERROR);
    assert(gpu_put==2 && display_put==3 && freed==2 && !s.mdl);
    fragment=0;duplicate=1;assert(a04_shared_scanout_prepare(&s,&ga,(void*)1,&da,(void*)2)==STATUS_DEVICE_CONFIGURATION_ERROR);
    assert(gpu_put==3 && display_put==4 && freed==3 && !s.mdl);
    assert(a04_shared_scanout_prepare(&s,&ga,(void*)1,&ga,(void*)2)==STATUS_INVALID_PARAMETER);
    return 0;
}
