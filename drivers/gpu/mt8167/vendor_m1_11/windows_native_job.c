/* SPDX-License-Identifier: MIT
 * Complete CPU producer -> real cold owner -> native shadow pair.
 * Device-memory copies are verified before firmware starts. No live submit
 * entry, gate switch, MMIO kick, KCCB publication or cleanup count update.
 */
#include "windows_native_job_private.h"
#include "A04BR3RogueDeviceWords.h"
#include "native_hwrt_profile.h"
#include "../windows_hw.h"
#define JOB_TAG 0x6a4e4750u
static int verified(volatile void *destination,const void *source,unsigned bytes)
{
    _Alignas(8) uint32_t read[64];const uint8_t *src=source;unsigned n,i;
    volatile uint8_t *dst=destination;
    while(bytes){
        n=bytes>sizeof(read)?sizeof(read):bytes;
        if((n&3u) || a04_rgx_device_write_words(dst,src,n))return 1;
        a04_gpu_dma_barrier();
        if(a04_rgx_device_read_words(read,dst,n))return 1;
        for(i=0;i<n;i++)if(((uint8_t *)read)[i]!=src[i])return 1;
        bytes-=n;src+=n;dst+=n;
    }
    return 0;
}
static int matches(volatile void *source,const void *expected,unsigned bytes)
{
    _Alignas(8) uint32_t read[64];volatile uint8_t *src=source;const uint8_t *exp=expected;unsigned n,i;
    while(bytes){
        n=bytes>sizeof(read)?sizeof(read):bytes;
        if((n&3u) || a04_rgx_device_read_words(read,src,n))return 1;
        for(i=0;i<n;i++)if(((uint8_t *)read)[i]!=exp[i])return 1;
        bytes-=n;src+=n;exp+=n;
    }
    return 0;
}
static volatile void *data_span(struct a04_vendor_native_cold *n,uint64_t gpu,unsigned bytes)
{
    if(gpu<A04_NATIVE_DATA_VA || gpu-A04_NATIVE_DATA_VA>n->used || bytes>n->used-(gpu-A04_NATIVE_DATA_VA))return NULL;
    return a04_vendor_native_storage_pointer(n,0,(ULONG)(gpu-A04_NATIVE_DATA_VA),bytes);
}
static int zeros(volatile void *source,unsigned bytes)
{
    _Alignas(8) uint32_t read[64];volatile uint8_t *src=source;unsigned n,i;
    while(bytes){
        n=bytes>sizeof(read)?sizeof(read):bytes;
        if((n&3u) || a04_rgx_device_read_words(read,src,n))return 1;
        for(i=0;i<n/4u;i++)if(read[i])return 1;
        bytes-=n;src+=n;
    }
    return 0;
}
static NTSTATUS prepare_job_cold(struct a04_vendor_native_cold *n,uint32_t pixel)
{
    struct a04_vendor_render_cold *r;struct a04_vendor_native_candidate *c;
    struct a04_m111_clear_job_input input={0};struct a04_m111_pair_input p={0};
    struct a04_m111_shadow_ring ta,three;RGXFWIF_CCCB_CTL ctl[2];
    _Alignas(8) struct a04_m111_hwrt_input profile[2];
    struct a04_m111_native_bank *expected_bank;struct a04_m111_native_addresses expected_addresses;
    RGXFWIF_FREELIST fl;struct a04_m111_freelist_input fl_input;uint32_t *entries;
    uint32_t native_sync[2];unsigned i;NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;
    if(!n || KeGetCurrentIrql()!=PASSIVE_LEVEL || n->candidate || n->quarantined ||
       !(r=n->render) || r->native_owner!=n || !r->firmware || r->firmware->hardware_owned ||
       r->poisoned || r->cleanup_started || !n->mapped || !n->prepared || !n->bank_prepared ||
       !n->backing_staged || !n->output_mapped || n->code_mapped!=3u ||
       n->width!=800u || n->height!=600u || r->buffer[0].bytes!=65536u ||
       n->suffix_offset>n->used || n->used-n->suffix_offset!=4096u ||
       !n->code[0].cpu || !n->code[1].cpu || !n->data.cpu || !a04_vendor_native_output_ready(n))
        return STATUS_INVALID_DEVICE_STATE;
    if(a04_m111_build_single_rt_hwrt_profile(profile,800,600,&n->target,&n->mlists))return status;
    if(!data_span(n,n->target.region_gpu[0],n->target.region_used) ||
       !data_span(n,n->target.tail_ptrs_gpu,n->target.tpc_vheap_used) ||
       !data_span(n,n->mlists.gpu[0],n->mlists.used) ||
       zeros(data_span(n,n->target.region_gpu[0],n->target.region_used),n->target.region_used) ||
       zeros(data_span(n,n->target.tail_ptrs_gpu,n->target.tpc_vheap_used),n->target.tpc_vheap_used) ||
       zeros(data_span(n,n->mlists.gpu[0],n->mlists.used),n->mlists.used))return status;
    if(n->auxiliary.dummy_region_gpu<A04_NATIVE_DATA_VA ||
       n->auxiliary.dummy_region_gpu-A04_NATIVE_DATA_VA>n->used-128u ||
       n->auxiliary.dummy_stencil_gpu<A04_NATIVE_DATA_VA ||
       n->auxiliary.dummy_stencil_gpu-A04_NATIVE_DATA_VA>n->used-2048u)return status;
    for(i=0;i<2;i++){
        if(r->layout.control[i]>65536u-sizeof(ctl[i]) || r->layout.commands[i]>65536u-4096u ||
           r->client_write[i] || a04_rgx_device_read_words(&ctl[i],
               (UCHAR *)r->buffer[0].cpu+r->layout.control[i],sizeof(ctl[i])) ||
           ctl[i].ui32WrapMask!=4095u || ctl[i].ui32ReadOffset || ctl[i].ui32WriteOffset ||
           ctl[i].ui32DepOffset)return status;
    }
    /* Verified Windows cold policy, NOT a claim about unread Android memory.
     * Matches same-version Linux fresh-PMR ZERO_ALL_PAGES/__GFP_ZERO and the
     * flags3 UM allocation path with no explicit CPU initialization. */
    if(zeros(data_span(n,n->auxiliary.dummy_region_gpu,128),128) ||
       zeros(data_span(n,n->auxiliary.dummy_stencil_gpu,2048),2048) ||
       a04_rgx_device_read_words(native_sync,(UCHAR *)r->buffer[0].cpu+0x4c00u,sizeof(native_sync)) ||
       native_sync[0] || native_sync[1])return status;
    a04_vendor_native_diagnose(n,A04_NATIVE_PREP_JOB,0,sizeof(*c)+8192u);
    c=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*c)+8192u,JOB_TAG);
    if(!c)return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(c,sizeof(*c)+8192u);
    /* Rebuild from the owner's real layout, compare the full cold FW bank,
     * and read back both PM tables including their reconstruction mirrors. */
    expected_bank=(void *)(((uintptr_t)c+sizeof(*c)+63u)&~(uintptr_t)63u);
    if(a04_m111_build_native_bank(expected_bank,&expected_addresses,0xc0084800u,n->target.vheap_gpu,
          n->freelist,profile) || matches((UCHAR *)r->buffer[0].cpu+0x4800u,expected_bank,sizeof(*expected_bank)))goto fail;
    if(expected_addresses.hwrt[0]!=n->addresses.hwrt[0] ||
       expected_addresses.ta_cleanup[0]!=n->addresses.ta_cleanup[0] ||
       expected_addresses.three_d_cleanup[0]!=n->addresses.three_d_cleanup[0])goto fail;
    entries=(void *)((uint8_t *)c+sizeof(*c)+2048u);
    for(i=0;i<2;i++){
        unsigned pages=i?256u:40u;
        fl_input=(struct a04_m111_freelist_input){A04_NATIVE_DATA_VA+n->list_offset[i],pages,i+1u};
        if(n->list_offset[i]>n->used || pages*8u>n->used-n->list_offset[i] ||
           a04_m111_build_freelist(&fl,entries,512u,&fl_input,n->pm_pages+(i?40:0),1) ||
           matches(a04_vendor_native_storage_pointer(n,0,n->list_offset[i],pages*8u),entries,pages*8u))goto fail;
    }
    input.output_gpu=a04_vendor_native_output_gpu(n);input.data_page_gpu=A04_NATIVE_DATA_VA+n->suffix_offset;
    input.usc_page_gpu=A04_NATIVE_USC_VA;input.pds_page_gpu=A04_NATIVE_PDS_VA;
    input.dummy_region_gpu=n->auxiliary.dummy_region_gpu;input.dummy_stencil_gpu=n->auxiliary.dummy_stencil_gpu;
    input.hwrt_fw=n->addresses.hwrt[0];input.pr_fence_fw=0xc0084c00u;
    /* Native PBE output is byte-addressed RGBA8. For D3DDDIFMT_A8R8G8B8
     * the desired memory bytes are B,G,R,A: use the packed DWORD verbatim,
     * not a semantic channel swap hidden in the DOD/copy transport. */
    for(i=0;i<4;i++)input.rgba[i]=(uint8_t)(pixel>>(i*8));
#if A04_GE8300_NATIVE_OWNER_MDL_TARGET
    c->clear_pixel=pixel;
#endif
    if(a04_m111_build_clear_job_800x600_cold(&c->job,&input))goto fail;
#if A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
    if(a04_m111_build_triangle_suite(&c->triangle,&input,(uint8_t *)c+sizeof(*c),8192u))goto fail;
    RtlCopyMemory(c->job.ta,c->triangle.record[0].ta,sizeof(c->job.ta));
    RtlCopyMemory(c->job.three,c->triangle.record[0].three,sizeof(c->job.three));
#endif
    p.ta_context=0xc0080000u+r->layout.context;
    p.three_d_context=p.ta_context+(uint32_t)offsetof(RGXFWIF_FWRENDERCONTEXT,s3DContext);
    p.hwrt=input.hwrt_fw;p.ta_sync=input.pr_fence_fw;p.three_d_sync=0xc0084c04u;
    p.three_d_update_value=1;p.external_job=1;p.internal_job=1;
    ta=(struct a04_m111_shadow_ring){c->rings,0,0};
    three=(struct a04_m111_shadow_ring){c->rings+4096,0,0};
    if(a04_m111_prepare_shadow_pair(&ta,&three,&p,c->job.ta,c->job.three,
          (uint8_t *)c+sizeof(*c),8192u,&c->pair))goto fail;
    if(
#if A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
       verified(n->code[0].cpu,c->triangle.usc,sizeof(c->triangle.usc)) ||
       verified(n->code[1].cpu,c->triangle.pds,sizeof(c->triangle.pds)) ||
       verified(a04_vendor_native_storage_pointer(n,0,n->suffix_offset,sizeof(c->triangle.data)),c->triangle.data,sizeof(c->triangle.data)) ||
#else
       verified(n->code[0].cpu,c->job.code.usc,sizeof(c->job.code.usc)) ||
       verified(n->code[1].cpu,c->job.code.pds,sizeof(c->job.code.pds)) ||
       verified(a04_vendor_native_storage_pointer(n,0,n->suffix_offset,sizeof(c->job.data.bytes)),c->job.data.bytes,sizeof(c->job.data.bytes)) ||
#endif
       verified((UCHAR *)r->buffer[0].cpu+r->layout.commands[0],c->rings,4096u) ||
       verified((UCHAR *)r->buffer[0].cpu+r->layout.commands[1],c->rings+4096,4096u)){
        /* Never free possibly staged DMA after an ambiguous write failure. */
        n->quarantined=TRUE;r->poisoned=TRUE;status=STATUS_IO_DEVICE_ERROR;goto fail;
    }
    a04_gpu_dma_barrier();n->candidate=c;return STATUS_SUCCESS;
fail:
    ExFreePoolWithTag(c,JOB_TAG);return status;
}
NTSTATUS a04_vendor_native_prepare_job_cold(struct a04_vendor_native_cold *n)
{ return prepare_job_cold(n,0xff332211u); }
#if A04_GE8300_NATIVE_OWNER_MDL_TARGET
NTSTATUS a04_vendor_native_prepare_job_cold_colour(struct a04_vendor_native_cold *n,uint32_t argb)
{ return prepare_job_cold(n,argb); }
#endif
void a04_vendor_native_free_job_cold(struct a04_vendor_native_cold *n)
{
    /* Called only by release_contents after hardware-owned/quarantine checks. */
    if(n && n->candidate){ExFreePoolWithTag(n->candidate,JOB_TAG);n->candidate=NULL;}
}
