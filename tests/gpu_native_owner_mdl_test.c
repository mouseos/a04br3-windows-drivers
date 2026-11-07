/* SPDX-License-Identifier: BSD-2-Clause-Patent. Host OS doubles only. */
#include <assert.h>
#include <stdlib.h>
#include "native_owner_mdl.h"
static unsigned irql,gets,puts,holds,drops,pools,flushes,fail_get,bounce_get;
unsigned KeGetCurrentIrql(void){return irql;}
void *ExAllocatePoolWithTag(int t,size_t n,unsigned tag){(void)t;(void)tag;void *p=calloc(1,n);if(p)pools++;return p;}
void ExFreePoolWithTag(void *p,unsigned tag){(void)tag;assert(pools);pools--;free(p);}
void *MmGetSystemAddressForMdlSafe(PMDL m,int p){(void)p;return m->va;}
void KeInitializeEvent(KEVENT *e,int t,BOOLEAN b){(void)t;e->signaled=b;}
void KeSetEvent(KEVENT *e,int inc,BOOLEAN w){(void)inc;(void)w;e->signaled=1;}
NTSTATUS KeWaitForSingleObject(void *e,int why,int mode,BOOLEAN alert,void *time)
{(void)why;(void)mode;(void)alert;(void)time;assert(((KEVENT *)e)->signaled);return STATUS_SUCCESS;}
void KeFlushIoBuffers(PMDL m,BOOLEAN read,BOOLEAN dma){assert(m && dma);(void)read;flushes++;}
static NTSTATUS retain(void *c,PMDL m){assert(c==m && !holds);holds++;return STATUS_SUCCESS;}
static void drop(void *c,PMDL m){assert(c==m && holds==1);holds--;drops++;}
static NTSTATUS get(PDMA_ADAPTER a,PDEVICE_OBJECT pdo,PMDL m,void *va,ULONG n,PDRIVER_LIST_CONTROL cb,void *ctx,BOOLEAN write)
{
    PSCATTER_GATHER_LIST sg;ULONG at=(ULONG)((UCHAR *)va-(UCHAR *)m->va);
    (void)a;assert(pdo && !write && n<=65536 && !(at&4095));gets++;
    if(gets==fail_get)return STATUS_INSUFFICIENT_RESOURCES;
    sg=calloc(1,sizeof(*sg));assert(sg);sg->NumberOfElements=1;
    sg->Elements[0].Address.QuadPart=(int64_t)m->pfns[at/4096]*4096;
    if(gets==bounce_get)sg->Elements[0].Address.QuadPart+=0x10000000;
    sg->Elements[0].Length=n;cb(pdo,NULL,sg,ctx);return STATUS_SUCCESS;
}
static void put(PDMA_ADAPTER a,PSCATTER_GATHER_LIST sg,BOOLEAN write){(void)a;assert(!write);puts++;free(sg);}
static void setup(void){irql=gets=puts=holds=drops=pools=flushes=fail_get=bounce_get=0;}
int main(void)
{
    DMA_OPERATIONS ops={.GetScatterGatherList=get,.PutScatterGatherList=put,.Size=sizeof(DMA_OPERATIONS)};
    struct fake_adapter adapter={&ops};PFN_NUMBER pfns[48];struct fake_mdl mdl={0};
    struct a04_native_mdl_binding *b=NULL;ULONG count;const uint64_t *pages;
    void *cpu=aligned_alloc(4096,48*4096);assert(cpu);
    for(unsigned i=0;i<48;i++)pfns[i]=0x10000+i+(i/16)*0x100;
    mdl.MdlFlags=MDL_PAGES_LOCKED;mdl.bytes=48*4096;mdl.va=cpu;mdl.pfns=pfns;
    struct a04_native_mdl_lease lease={&mdl,retain,drop};
    setup();assert(a04_native_mdl_bind(&b,&adapter,&mdl,&mdl,mdl.bytes,&lease)==STATUS_SUCCESS);
    assert(holds==1 && gets==3 && a04_native_mdl_original(b)==&mdl && a04_native_mdl_cpu(b)==cpu);
    pages=a04_native_mdl_pages(b,&count);assert(count==48);
    for(unsigned i=0;i<48;i++)assert(pages[i]==(uint64_t)pfns[i]*4096);
    assert(a04_native_mdl_sync_for_gpu(b)==STATUS_SUCCESS && flushes==1);
    assert(a04_native_mdl_release_cold(&b)==STATUS_SUCCESS && !b && !holds && drops==1 && puts==3 && !pools);
    for(unsigned i=1;i<=3;i++){
        setup();fail_get=i;assert(!NT_SUCCESS(a04_native_mdl_bind(&b,&adapter,&mdl,&mdl,mdl.bytes,&lease)));
        assert(!b && !holds && drops==1 && puts==i-1 && !pools);
        setup();bounce_get=i;assert(!NT_SUCCESS(a04_native_mdl_bind(&b,&adapter,&mdl,&mdl,mdl.bytes,&lease)));
        assert(!b && !holds && drops==1 && puts==i && !pools);
    }
    setup();mdl.MdlFlags|=MDL_IO_SPACE;
    assert(!NT_SUCCESS(a04_native_mdl_bind(&b,&adapter,&mdl,&mdl,mdl.bytes,&lease)) && !gets && !holds);
    mdl.MdlFlags=MDL_PAGES_LOCKED;
    assert(a04_native_mdl_bind(&b,&adapter,&mdl,&mdl,mdl.bytes,&lease)==STATUS_SUCCESS);
    assert(a04_native_mdl_mark_exposed(b)==STATUS_SUCCESS);
    assert(a04_native_mdl_sync_for_cpu(b)==STATUS_SUCCESS && flushes==1);
    assert(a04_native_mdl_release_cold(&b)==STATUS_DEVICE_BUSY && b && holds && !puts);
    /* Exposed mapping intentionally remains retained; no fake drain proof. */
    return 0;
}
