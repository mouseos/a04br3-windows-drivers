/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include "mt8167_brightness.hxx"
static ULONG regs[1024],baseline[1024];static unsigned maps,unmaps,pools,writes,irql;
static bool reject_write;
extern "C" {
ULONG KeGetCurrentIrql(void){return irql;}
void KeInitializeMutex(KMUTEX*m,ULONG){m->held=0;}
NTSTATUS KeWaitForSingleObject(void*m,int,int,BOOLEAN,void*){KMUTEX*k=(KMUTEX*)m;assert(!k->held);k->held=1;return STATUS_SUCCESS;}
void KeReleaseMutex(KMUTEX*m,BOOLEAN){assert(m->held);m->held=0;}
void* ExAllocatePoolWithTag(int,size_t n,ULONG){void*p=calloc(1,n);if(p)pools++;return p;}
void ExFreePoolWithTag(void*p,ULONG){assert(pools);pools--;free(p);}
void* MmMapIoSpace(PHYSICAL_ADDRESS p,size_t n,int){assert(p.QuadPart==0x1100f000 && n==4096);maps++;return regs;}
void MmUnmapIoSpace(void*p,size_t n){assert(p==regs && n==4096);unmaps++;}
ULONG READ_REGISTER_ULONG(volatile ULONG*p){assert(p>=regs && p<regs+1024);return *p;}
void WRITE_REGISTER_ULONG(volatile ULONG*p,ULONG v){assert(p==regs+5 || p==regs+2);writes++;if(!reject_write)*p=v;}
}
static const GUID guid={0xfde5bba4,0xb3f9,0x46fb,{0xbd,0xaa,0x07,0x28,0xce,0x31,0x00,0xb4}};
static QUERY_INTERFACE query(DXGK_BRIGHTNESS_INTERFACE *b)
{QUERY_INTERFACE q={&guid,sizeof(*b),1,(PINTERFACE)b,NULL};return q;}
static void setup(CM_RESOURCE_LIST *r)
{
    memset(r,0,sizeof(*r));r->Count=1;r->List[0].PartialResourceList.Count=1;
    CM_PARTIAL_RESOURCE_DESCRIPTOR &d=r->List[0].PartialResourceList.PartialDescriptors[0];
    d.Type=CmResourceTypeMemory;d.ShareDisposition=CmResourceShareDeviceExclusive;d.u.Memory.Start.QuadPart=0x1100f000;d.u.Memory.Length=4096;
    for(unsigned i=0;i<1024;i++)regs[i]=0x12340000u+i;
    regs[0]=1;regs[2]=0;regs[4]=0x30000;regs[5]=0xa0000000u|(512u<<16)|1023u;
    memcpy(baseline,regs,sizeof(regs));
}
int main()
{
    A04Mt8167Brightness owner;owner.Initialize();CM_RESOURCE_LIST r;DXGK_BRIGHTNESS_INTERFACE b={};QUERY_INTERFACE q=query(&b);
    assert(owner.QueryInterface(&q)==STATUS_NOT_SUPPORTED);
    setup(&r);r.Count=0;assert(owner.Start(&r)==STATUS_NOT_SUPPORTED && !maps);
    setup(&r);r.List[0].PartialResourceList.PartialDescriptors[0].ShareDisposition=0;
    assert(!NT_SUCCESS(owner.Start(&r)) && !maps);
    setup(&r);r.List[0].PartialResourceList.Count=2;r.List[0].PartialResourceList.PartialDescriptors[1]=r.List[0].PartialResourceList.PartialDescriptors[0];
    assert(!NT_SUCCESS(owner.Start(&r)) && !maps);
    setup(&r);regs[0]=0;assert(!NT_SUCCESS(owner.Start(&r)) && pools==0 && maps==unmaps && !writes);
    setup(&r);assert(owner.Start(&r)==STATUS_SUCCESS && !writes);
    q.Version=2;assert(owner.QueryInterface(&q)==STATUS_NOT_SUPPORTED);q.Version=1;
    q.Size--;assert(owner.QueryInterface(&q)==STATUS_NOT_SUPPORTED);q.Size++;
    assert(owner.QueryInterface(&q)==STATUS_SUCCESS && pools==1);
    UCHAR level=0,count=0,levels[100]={};
    assert(b.GetPossibleBrightness(b.Context,0,&count,NULL)==STATUS_BUFFER_TOO_SMALL && count==100);
    assert(b.GetPossibleBrightness(b.Context,100,&count,levels)==STATUS_SUCCESS && levels[0]==1 && levels[99]==100);
    assert(b.GetBrightness(b.Context,&level)==STATUS_SUCCESS && level==50);
    assert(!NT_SUCCESS(b.SetBrightness(b.Context,0)) && !NT_SUCCESS(b.SetBrightness(b.Context,101)) && !writes);
    for(unsigned i=1;i<=100;i++){
        assert(b.SetBrightness(b.Context,(UCHAR)i)==STATUS_SUCCESS);
        assert(b.GetBrightness(b.Context,&level)==STATUS_SUCCESS && level==i);
        assert((regs[5]&~(0x1fffu<<16))==(baseline[5]&~(0x1fffu<<16)));
    }
    for(unsigned i=0;i<1024;i++)if(i!=5)assert(regs[i]==baseline[i]);
    unsigned before=writes;owner.SetPower(FALSE);
    assert(b.SetBrightness(b.Context,20)==STATUS_DEVICE_NOT_READY && writes==before);
    owner.SetPower(TRUE);assert(b.GetBrightness(b.Context,&level)==STATUS_SUCCESS);
    regs[4]^=1;assert(!NT_SUCCESS(b.SetBrightness(b.Context,20)) && writes==before);regs[4]^=1;
    reject_write=true;assert(b.SetBrightness(b.Context,20)==STATUS_IO_DEVICE_ERROR);reject_write=false;
    before=writes;assert(b.SetBrightness(b.Context,30)==STATUS_DEVICE_NOT_READY && writes==before);
    b.InterfaceReference(b.Context);owner.Stop();assert(maps==unmaps && pools==1);
    assert(b.GetBrightness(b.Context,&level)==STATUS_DEVICE_NOT_READY);
    b.InterfaceDereference(b.Context);assert(pools==1);b.InterfaceDereference(b.Context);assert(!pools);
    setup(&r);assert(owner.Start(&r)==STATUS_SUCCESS);owner.Stop();assert(!pools && maps==unmaps);
    puts("MT8167 brightness DDI/resource/power/reference tests passed");
}
