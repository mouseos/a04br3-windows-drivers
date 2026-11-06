/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native cold backing contract: NATIVE_COLD_CONTRACT.md / native_resources.
 * PM physical-page protection follows m1.11 rgxta3d.c RGXGrowFreeList's
 * PMMETA_PROTECT allocation. No drawing packet, MMIO kick or FW execution. */
#include "windows_native_cold_private.h"
#include "native_hwrt_profile.h"
#include "windows_copy_scratch.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
#include "../gpu_mmu.h"
#include "../windows_hw.h"
#define TAG 0x6e47504du
#define ROUND_PAGE(n) (((n)+4095u)&~4095u)
_Static_assert(sizeof(struct a04_m111_native_bank)==0x400,"native FW bank ABI");
_Static_assert(offsetof(struct a04_m111_native_bank,freelist)==0x40,"freelist ABI");
_Static_assert(offsetof(struct a04_m111_native_bank,hwrt)==0xc0,"HWRT ABI");
static int overlap_all(const struct a04_vendor_uncached *a,
                       const struct a04_vendor_uncached *b)
{
    return b->cpu && (a04_rgx_fw_overlap((uint64_t)a->logical.QuadPart,a->bytes,
               (uint64_t)b->logical.QuadPart,b->bytes) ||
           a04_rgx_fw_alias(a->cpu,a->bytes,b->cpu,b->bytes));
}
static int owned_range(struct a04_vendor_native_cold *n,const struct a04_vendor_uncached *a)
{
    unsigned i;struct a04_vendor_render_cold *r=n->render;
    struct a04_vendor_m1_11_memory *f=r->firmware;
    struct a04_vendor_copy_scratch *copy=r->copy_owner;
    if(!a->cpu || a->logical.QuadPart<=0 || (a->logical.QuadPart&4095) ||
       (uint64_t)a->logical.QuadPart>UINT64_C(0x100000000)-a->bytes ||
       ((uintptr_t)a->cpu&7u))return 0;
    for(i=0;i<16;i++)if(overlap_all(a,&r->buffer[i]))return 0;
    for(i=0;i<5;i++)if(a04_rgx_fw_overlap((uint64_t)a->logical.QuadPart,a->bytes,
        f->buffers[i].dma,f->buffers[i].bytes) ||
        a04_rgx_fw_alias(a->cpu,a->bytes,f->buffers[i].cpu,f->buffers[i].bytes))return 0;
    if(copy && (overlap_all(a,&copy->source) || overlap_all(a,&copy->destination) ||
                overlap_all(a,&copy->shared)))return 0;
    return 1;
}
static NTSTATUS release_contents(struct a04_vendor_native_cold *n)
{
    uint64_t zero=0;struct a04_vendor_render_cold *r;
    if(!n || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    r=n->render;
    if(r && (!r->firmware || r->firmware->hardware_owned))return STATUS_DEVICE_BUSY;
    if(r && r->native_owner!=n)return STATUS_INVALID_DEVICE_STATE;
    if(n->code_mapped) {
        NTSTATUS code_status=a04_vendor_native_unmap_code_cold(n);
        if(!NT_SUCCESS(code_status))return code_status;
    }
    if(n->mapped) {
        /* Only our General-window directory entry was added. Never restore
         * an old whole-directory snapshot over another child's cold mapping. */
        if(!r || a04_rgx_device_write_words((UCHAR *)r->buffer[2].cpu+
            (((A04_NATIVE_DATA_VA>>21)&511u)*8u),&zero,sizeof(zero)))
            return STATUS_INVALID_DEVICE_STATE;
        a04_gpu_dma_barrier();n->mapped=FALSE;
    }
    if(n->bank_touched) {
        uint32_t *zeros=ExAllocatePoolWithTag(NonPagedPoolNx,0x440u,TAG);
        if(!zeros)return STATUS_INSUFFICIENT_RESOURCES;
        RtlZeroMemory(zeros,0x440u);
        if(a04_rgx_device_write_words((UCHAR *)r->buffer[0].cpu+0x4800u,zeros,0x440u)) {
            ExFreePoolWithTag(zeros,TAG);return STATUS_INVALID_DEVICE_STATE;
        }
        ExFreePoolWithTag(zeros,TAG);a04_gpu_dma_barrier();n->bank_prepared=FALSE;n->bank_touched=FALSE;
    }
    a04_vendor_uncached_free(&n->code[1]);a04_vendor_uncached_free(&n->code[0]);
    a04_vendor_uncached_free(&n->pt);a04_vendor_uncached_free(&n->data);
    if(r)r->native_owner=NULL;
    RtlZeroMemory(n,sizeof(*n));return STATUS_SUCCESS;
}
static NTSTATUS prepare_contents(struct a04_vendor_native_cold *n,
    struct a04_vendor_render_cold *r,PDMA_ADAPTER adapter,ULONG map_registers,
    ULONG width,ULONG height)
{
    UCHAR *shadow=NULL;uint32_t *pc;uint64_t *pd,*pt;ULONG region,tpc,tiles_x,tiles_y,side=1,at;
    unsigned i;uint64_t address;NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;
    struct a04_m111_freelist_input fl;
    if(!n || !r || !adapter || KeGetCurrentIrql()!=PASSIVE_LEVEL || n->render || n->data.cpu ||
       n->pt.cpu || n->prepared || n->mapped || !r->mapped || !r->firmware ||
       !r->firmware->staged || !r->firmware->uncached || r->firmware->hardware_owned ||
       r->firmware->adapter!=adapter || r->native_owner || r->layout.used>0x4800u ||
       r->buffer[0].bytes<0x4c40u || width!=800u || (height!=600u && height!=1280u))
        return STATUS_INVALID_PARAMETER;
    tiles_x=(width+15u)/16u;tiles_y=(height+15u)/16u;
    region=ROUND_PAGE(((tiles_x*tiles_y*5u+63u)&~63u)*2u);
    while(side<tiles_x || side<tiles_y)side*=2u;
    tpc=ROUND_PAGE(((side*side*8u+63u)&~63u)+0x600u);
    at=region+tpc;
    if(a04_m111_layout_target_storage(width,height,A04_NATIVE_DATA_VA,region,
          A04_NATIVE_DATA_VA+region,tpc,&n->target) ||
       a04_m111_layout_pm_mlists(0x100000u,A04_NATIVE_DATA_VA+at,0x2000u,&n->mlists))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    at+=0x2000u;
    if(a04_m111_layout_auxiliary(A04_NATIVE_DATA_VA+at,4096u,&n->auxiliary))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    at+=4096u;n->pm_offset[0]=at;at+=0x28000u;
    n->pm_offset[1]=at;at+=0x100000u;
    n->list_offset[0]=at;at+=4096u;n->list_offset[1]=at;at+=4096u;
    if(at>A04_NATIVE_DATA_LIMIT || map_registers<at/4096u+1u)return STATUS_INSUFFICIENT_RESOURCES;
    n->used=at;n->width=width;n->height=height;n->render=r;r->native_owner=n;
    status=a04_vendor_uncached_allocate(&n->data,adapter,at);
    if(!NT_SUCCESS(status))goto done;
    status=a04_vendor_uncached_allocate(&n->pt,adapter,4096u);
    if(!NT_SUCCESS(status))goto done;
    status=STATUS_DEVICE_CONFIGURATION_ERROR;
    if(!owned_range(n,&n->data) || !owned_range(n,&n->pt) || overlap_all(&n->data,&n->pt))goto done;
    shadow=ExAllocatePoolWithTag(NonPagedPoolNx,(size_t)at+3u*4096u,TAG);
    if(!shadow){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
    /* TPC/VHeap zero is vendor-defined. Other zero-filled storage is only
     * a host allocation policy; the future complete-record factory must
     * supply any required region/MLIST/auxiliary initial contents. */
    RtlZeroMemory(shadow,(size_t)at+3u*4096u);
    pc=(uint32_t *)(void *)(shadow+at);pd=(uint64_t *)(void *)(shadow+at+4096u);
    pt=(uint64_t *)(void *)(shadow+at+8192u);
    if(a04_rgx_device_read_words(pc,r->buffer[1].cpu,4096u) ||
       a04_rgx_device_read_words(pd,r->buffer[2].cpu,4096u) ||
       pc[A04_NATIVE_DATA_VA>>30]!=((uint32_t)((uint64_t)r->buffer[2].logical.QuadPart>>8)|1u) ||
       pd[(A04_NATIVE_DATA_VA>>21)&511u])goto done;
    address=(uint64_t)n->data.logical.QuadPart;
    for(i=0;i<40u;i++)n->pm_pages[i]=address+n->pm_offset[0]+i*4096u;
    for(i=0;i<256u;i++)n->pm_pages[40u+i]=address+n->pm_offset[1]+i*4096u;
    fl.list_gpu=A04_NATIVE_DATA_VA+n->list_offset[0];fl.pages=40;fl.id=1;
    if(a04_m111_build_freelist(&n->freelist[0],(uint32_t *)(void *)(shadow+n->list_offset[0]),
        1024u,&fl,n->pm_pages,1))goto done;
    fl.list_gpu=A04_NATIVE_DATA_VA+n->list_offset[1];fl.pages=256;fl.id=2;
    if(a04_m111_build_freelist(&n->freelist[1],(uint32_t *)(void *)(shadow+n->list_offset[1]),
        1024u,&fl,n->pm_pages+40,1))goto done;
    if(a04_gpu_mmu_map_4k(pc,pd,pt,(uint64_t)r->buffer[2].logical.QuadPart,
          (uint64_t)n->pt.logical.QuadPart,A04_NATIVE_DATA_VA,address,n->pm_offset[0]/4096u,A04_GPU_MMU_SLC_BYPASS) ||
       a04_gpu_mmu_map_4k(pc,pd,pt,(uint64_t)r->buffer[2].logical.QuadPart,
          (uint64_t)n->pt.logical.QuadPart,A04_NATIVE_DATA_VA+n->pm_offset[0],address+n->pm_offset[0],296u,
          A04_GPU_MMU_SLC_BYPASS|A04_GPU_MMU_PM_FW_PROTECT) ||
       a04_gpu_mmu_map_4k(pc,pd,pt,(uint64_t)r->buffer[2].logical.QuadPart,
          (uint64_t)n->pt.logical.QuadPart,A04_NATIVE_DATA_VA+n->list_offset[0],address+n->list_offset[0],2u,A04_GPU_MMU_SLC_BYPASS))goto done;
    if(a04_rgx_device_write_words(n->data.cpu,shadow,at) ||
       a04_rgx_device_write_words(n->pt.cpu,pt,4096u))goto done;
    a04_gpu_dma_barrier();
    /* Existing PC/PD must already name this parent directory. Publish only
     * the formerly empty leaf entry; FW has not been started at this point. */
    if(a04_rgx_device_write_words((UCHAR *)r->buffer[2].cpu+(((A04_NATIVE_DATA_VA>>21)&511u)*8u),
        pd+((A04_NATIVE_DATA_VA>>21)&511u),8u))goto done;
    a04_gpu_dma_barrier();n->mapped=TRUE;n->prepared=TRUE;status=STATUS_SUCCESS;
done:
    if(shadow)ExFreePoolWithTag(shadow,TAG);
    if(!NT_SUCCESS(status))(void)release_contents(n);
    return status;
}

NTSTATUS a04_vendor_native_bind_bank_cold(struct a04_vendor_native_cold *n,
    const struct a04_m111_hwrt_input *hwrt)
{
    UCHAR *allocation;struct a04_m111_native_bank *bank;
    struct a04_m111_native_addresses addresses;unsigned i;
    const uint32_t sync[16]={0};NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;
    if(!n || !hwrt || KeGetCurrentIrql()!=PASSIVE_LEVEL || !n->prepared ||
       !n->mapped || n->bank_prepared || !n->render || n->render->native_owner!=n ||
       !n->render->firmware || n->render->firmware->hardware_owned)return STATUS_INVALID_DEVICE_STATE;
    for(i=0;i<2;i++)if(hwrt[i].pm_mlist!=n->mlists.gpu[i] ||
       hwrt[i].tail_ptrs!=n->target.tail_ptrs_gpu || hwrt[i].region_header!=n->target.region_gpu[i] ||
       hwrt[i].macrotile || hwrt[i].rtc)return STATUS_INVALID_PARAMETER;
    allocation=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*bank)+63u,TAG);
    if(!allocation)return STATUS_INSUFFICIENT_RESOURCES;
    bank=(struct a04_m111_native_bank *)(void *)(((uintptr_t)allocation+63u)&~(uintptr_t)63u);
    if(a04_m111_build_native_bank(bank,&addresses,0xc0084800u,n->target.vheap_gpu,n->freelist,hwrt)) {
        ExFreePoolWithTag(allocation,TAG);return status;
    }
    n->bank_touched=TRUE;
    if(!a04_rgx_device_write_words((UCHAR *)n->render->buffer[0].cpu+0x4800u,bank,sizeof(*bank)) &&
       !a04_rgx_device_write_words((UCHAR *)n->render->buffer[0].cpu+0x4c00u,sync,sizeof(sync))) {
        a04_gpu_dma_barrier();n->addresses=addresses;n->bank_prepared=TRUE;status=STATUS_SUCCESS;
    }
    ExFreePoolWithTag(allocation,TAG);return status;
}

ULONG a04_vendor_native_bytes(const struct a04_vendor_native_cold *n)
{ return n ? n->used : 0; }
NTSTATUS a04_vendor_native_allocate_cold(struct a04_vendor_native_cold **out,
    struct a04_vendor_render_cold *r,PDMA_ADAPTER adapter,ULONG maps,ULONG width,ULONG height)
{
    struct a04_vendor_native_cold *n;NTSTATUS status;
    _Alignas(8) struct a04_m111_hwrt_input profile[2];
    if(!out || *out || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    n=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*n),TAG);
    if(!n)return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(n,sizeof(*n));status=prepare_contents(n,r,adapter,maps,width,height);
    if(NT_SUCCESS(status)) {
        if(a04_m111_build_single_rt_hwrt_profile(profile,width,height,&n->target,&n->mlists))
            status=STATUS_DEVICE_CONFIGURATION_ERROR;
        else status=a04_vendor_native_bind_bank_cold(n,profile);
        if(NT_SUCCESS(status))status=a04_vendor_native_reserve_code_cold(n,adapter);
        if(!NT_SUCCESS(status))(void)release_contents(n);
    }
    if(!NT_SUCCESS(status) && !n->render)ExFreePoolWithTag(n,TAG);
    else *out=n; /* Preserve a failed cold cleanup's ownership and allocation. */
    return status;
}
NTSTATUS a04_vendor_native_release_cold(struct a04_vendor_native_cold **owner)
{
    NTSTATUS status;
    if(!owner || !*owner)return STATUS_INVALID_PARAMETER;
    status=release_contents(*owner);
    if(NT_SUCCESS(status)){ExFreePoolWithTag(*owner,TAG);*owner=NULL;}
    return status;
}
