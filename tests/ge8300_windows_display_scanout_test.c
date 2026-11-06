/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/windows_display_scanout.h"
static SCATTER_GATHER_LIST list;
static int puts, readiness, boundary_count;
void a04_gpu_dma_barrier(void) {}
unsigned KeGetCurrentIrql(void) { return PASSIVE_LEVEL; }
void KeInitializeEvent(KEVENT *e,int t,BOOLEAN b) { (void)t;(void)b;e->signaled=0; }
void KeSetEvent(KEVENT *e,int p,BOOLEAN b) { (void)p;(void)b;e->signaled=1; }
NTSTATUS KeWaitForSingleObject(void *p,int a,int b,BOOLEAN c,void *d)
{ (void)a;(void)b;(void)c;(void)d;assert(((KEVENT *)p)->signaled);return STATUS_SUCCESS; }
PHYSICAL_ADDRESS MmGetPhysicalAddress(void *p)
{ PHYSICAL_ADDRESS a; a.QuadPart=(int64_t)(uintptr_t)p+0x40000000;return a; }
static NTSTATUS get(PDMA_ADAPTER a,PDEVICE_OBJECT d,PMDL m,void *v,ULONG n,PDRIVER_LIST_CONTROL cb,void *ctx,BOOLEAN write)
{ (void)a;(void)m;(void)v;(void)n;assert(!write);cb(d,0,&list,ctx);return STATUS_SUCCESS; }
static void put(PDMA_ADAPTER a,PSCATTER_GATHER_LIST s,BOOLEAN write)
{ (void)a;assert(s==&list && !write);puts++; }
NTSTATUS a04_vendor_surface_mark_exposed(struct a04_vendor_surface *s)
{ assert(!s->hardware_exposed);s->hardware_exposed=TRUE;return STATUS_SUCCESS; }
static NTSTATUS ready(void *ctx,uint64_t f) { (void)ctx;assert(f==7);return readiness ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY; }
static NTSTATUS boundary(void *ctx) { (void)ctx;boundary_count++;return STATUS_SUCCESS; }
int main(void)
{
    PFN_NUMBER pages[1000]; ULONG regs[0x1000/4]={0}; unsigned i;
    DMA_OPERATIONS ops={0}; struct fake_adapter adapter={&ops};
    struct fake_mdl mdl={0}; struct a04_vendor_surface surf={0}; struct a04_display_scanout s={0};
    ops.GetScatterGatherList=get;ops.PutScatterGatherList=put;
    surf.prepared=TRUE;surf.width=800;surf.height=1280;surf.pitch=3200;surf.bytes=4096000;surf.pixels.cpu=(void *)0x100000;surf.pixels.adapter=&adapter;surf.pixels.bytes=surf.bytes;
    for(i=0;i<1000;i++)pages[i]=(0x40100000u+i*4096u)>>12;
    mdl.MdlFlags=MDL_PAGES_LOCKED;mdl.bytes=4096000;mdl.va=surf.pixels.cpu;mdl.pfns=pages;
    list.NumberOfElements=2;list.Elements[0].Address.QuadPart=0x30000000;list.Elements[0].Length=4096;
    list.Elements[1].Address.QuadPart=0x30001000;list.Elements[1].Length=4096000-4096;
    pages[9]++;assert(a04_display_scanout_map(&s,&surf,&adapter,(void *)1,&mdl)==STATUS_DEVICE_CONFIGURATION_ERROR);pages[9]--;
    list.Elements[1].Address.QuadPart++;assert(a04_display_scanout_map(&s,&surf,&adapter,(void *)1,&mdl)==STATUS_DEVICE_CONFIGURATION_ERROR);assert(puts==1);list.Elements[1].Address.QuadPart--;
    assert(NT_SUCCESS(a04_display_scanout_map(&s,&surf,&adapter,(void *)1,&mdl)));
    regs[0x00c/4]=1;regs[0x030/4]=2u<<12;regs[0x02c/4]=9;regs[0x038/4]=(1280u<<16)|800;regs[0x044/4]=3200;regs[0xf40/4]=0x11100000;
    assert(a04_display_scanout_switch(&s,regs,sizeof(regs),0x11100000,7,ready,boundary,0)==STATUS_DEVICE_NOT_READY);
    assert(!boundary_count && regs[0xf40/4]==0x11100000);
    readiness=1;assert(a04_display_scanout_switch(&s,regs,sizeof(regs),0x11100000,7,ready,boundary,0)==STATUS_DEVICE_CONFIGURATION_ERROR);
    assert(!s.attempted && regs[0xf40/4]==0x11100000);
    regs[0x02c/4]=1;regs[0x030/4]|=1u<<9;
    assert(a04_display_scanout_switch(&s,regs,sizeof(regs),0x11100000,7,ready,boundary,0)==STATUS_DEVICE_CONFIGURATION_ERROR);
    regs[0x030/4]=2u<<12;
    assert(NT_SUCCESS(a04_display_scanout_switch(&s,regs,sizeof(regs),0x11100000,7,ready,boundary,0)));
    assert(regs[0xf40/4]==0x30000000 && s.attempted && surf.hardware_exposed);
    assert(a04_display_scanout_unmap(&s)==STATUS_DEVICE_BUSY && puts==1);
    return 0;
}
