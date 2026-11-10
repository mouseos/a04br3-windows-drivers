/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_native_copy_storage.h"
#include "windows_native_cold_private.h"
#include "native_owner_mdl.h"
#include "native_copy_usc.h"
#include "native_copy_region_usc_generated.h"
#include "A04BR3RogueDeviceWords.h"
#include "../../gpu_mmu.h"
#include "../../windows_hw.h"
#include "../../wddm-render/clear_protocol.h"
#include <string.h>
#define TAG 0x63434741u
/* Existing DOD heap bases. The native TA/3D code occupies leaf16 instead.
 * Leaf0 must be EMPTY in the actual cold tables before this child binds. */
static const uint64_t va[2]={UINT64_C(0xba00000000),UINT64_C(0xa800000000)};
static const ULONG dir[2]={8,5},leaf[2]={9,6};
struct a04_native_copy_storage {
 struct a04_vendor_native_cold *native;
 struct a04_native_mdl_binding *source,*destination;
 struct a04_vendor_uncached code[2];
 struct a04_native_copy_job_image image,clear_image;
 struct a04_native_copy_job_input input;
 ULONG mapped;BOOLEAN prepared,quarantined;
};
static int intersects(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{return a<=b?b-a<n:a-b<m;}
static NTSTATUS check_binding(struct a04_vendor_native_cold *n,
 struct a04_native_mdl_binding *b,uint64_t offset)
{
 ULONG count,i;const uint64_t *pages=a04_native_mdl_pages(b,&count);uint64_t entry;
 if(!pages||!count||count>1024u||a04_native_mdl_extent(b)!=count*4096u||
    a04_native_mdl_adapter(b)!=n->render->firmware->adapter||(offset&4095u)||
    offset>A04_NATIVE_APERTURE_BYTES-count*4096u)return STATUS_INVALID_PARAMETER;
 for(i=0;i<count;i++){
  ULONG index=(ULONG)(offset/4096u)+i;
  if(a04_rgx_device_read_words(&entry,(UCHAR*)n->output[2+index/512].cpu+(index%512)*8u,8u)||
     entry!=(pages[i]|A04_GPU_MMU_SLC_BYPASS|1u))return STATUS_DEVICE_CONFIGURATION_ERROR;
 }
 return STATUS_SUCCESS;
}
static int allocation_clashes(struct a04_vendor_uncached *a,struct a04_native_mdl_binding *b)
{
 ULONG n,i;const uint64_t *p=a04_native_mdl_pages(b,&n);
 if(!p||a04_rgx_fw_alias(a->cpu,a->bytes,a04_native_mdl_cpu_identity(b),a04_native_mdl_extent(b)))return 1;
 for(i=0;i<n;i++)if(intersects((uint64_t)a->logical.QuadPart,a->bytes,p[i],4096))return 1;
 return 0;
}
static int verified(void *dst,const void *source,ULONG bytes)
{
 _Alignas(8) ULONG got[16];const UCHAR *p=source;UCHAR *q=dst;ULONG n;
 while(bytes){n=bytes>sizeof(got)?sizeof(got):bytes;
  if(a04_rgx_device_write_words(q,p,n))return 1;a04_gpu_dma_barrier();
  if(a04_rgx_device_read_words(got,q,n)||memcmp(got,p,n))return 1;
  p+=n;q+=n;bytes-=n;
 }return 0;
}
NTSTATUS a04_native_copy_storage_release_cold(struct a04_native_copy_storage **owner)
{
 struct a04_native_copy_storage *c;struct a04_vendor_native_cold *n;struct a04_vendor_render_cold *r;
 uint64_t entry,zero=0,expected;ULONG i;
 if(!owner||!(c=*owner)||!(n=c->native)||!(r=n->render)||KeGetCurrentIrql()!=PASSIVE_LEVEL)
  return STATUS_INVALID_PARAMETER;
 /* Firmware exposure is not undone by a mask, fence or nominal completion.
  * The factory must prove hardware-off and retire the graph first. */
 if(c->quarantined||!r->firmware||r->firmware->hardware_owned)return STATUS_DEVICE_BUSY;
 for(i=0;i<2;i++)if(c->mapped&(1u<<i)){
  expected=(uint64_t)c->code[i].logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u|(i?0u:A04_GPU_MMU_READ_ONLY);
  if(a04_rgx_device_read_words(&entry,r->buffer[leaf[i]].cpu,8u)||entry!=expected||
     a04_rgx_device_write_words(r->buffer[leaf[i]].cpu,&zero,8u))goto quarantine;
  a04_gpu_dma_barrier();
  if(a04_rgx_device_read_words(&entry,r->buffer[leaf[i]].cpu,8u)||entry)goto quarantine;
  c->mapped&=~(1u<<i);
 }
 for(i=0;i<2;i++)a04_vendor_uncached_free(&c->code[i]);
 ExFreePoolWithTag(c,TAG);*owner=NULL;return STATUS_SUCCESS;
quarantine:c->quarantined=TRUE;n->quarantined=TRUE;return STATUS_IO_DEVICE_ERROR;
}
NTSTATUS a04_native_copy_storage_create(struct a04_native_copy_storage **out,
 struct a04_vendor_native_cold *n,struct a04_native_mdl_binding *destination,
 const struct geo_surface_desc *description,const void *usc,ULONG usc_bytes)
{
 struct a04_vendor_render_cold *r;struct a04_native_copy_storage *c;
 struct a04_native_copy_job_input in={0};ULONG i,j;uint64_t offset=0,entry,pd;
 struct a04_native_clear_job_input clear_in={0};
 uint32_t pc;NTSTATUS status=STATUS_INVALID_DEVICE_STATE;
 _Alignas(8) struct a04_vendor_clear_image eot;
 if(!out||*out||!n||!destination||!description||description->size!=sizeof(*description)||
    !geo_desc_valid(description)||!geo_render_layout_valid(description->width,description->height,description->pitch)||(description->format!=GEO_FORMAT_A8R8G8B8&&description->format!=GEO_FORMAT_X8R8G8B8)||
    (description->usage!=GEO_USAGE_STAGING&&!geo_usage_renderable(description->usage))||description->reserved||!usc||KeGetCurrentIrql()!=PASSIVE_LEVEL||
    !(r=n->render)||r->native_owner!=n||!r->firmware||r->firmware->hardware_owned||
    n->quarantined||r->poisoned||!n->registered_target||!n->output_mapped||n->code_mapped!=3u||
    !r->buffer[1].cpu||!n->output[2].cpu||!n->output[3].cpu||
    !a04_vendor_copy_usc_match(usc,usc_bytes))return STATUS_INVALID_PARAMETER;
 for(i=0;i<n->linear_count;i++)if(n->linear[i].binding==destination){offset=n->linear[i].offset;break;}
 if(i==n->linear_count||destination==n->registered_target||
    intersects(n->target_offset,a04_native_mdl_extent(n->registered_target),offset,a04_native_mdl_extent(destination)))return STATUS_INVALID_PARAMETER;
 status=check_binding(n,n->registered_target,n->target_offset);if(!NT_SUCCESS(status))return status;
 if(a04_native_mdl_extent(destination)!=((description->pitch*description->height+4095u)&~4095u))return STATUS_INVALID_PARAMETER;
 status=check_binding(n,destination,offset);if(!NT_SUCCESS(status))return status;
 c=ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*c),TAG);if(!c)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(c,sizeof(*c));c->native=n;c->source=n->registered_target;c->destination=destination;*out=c;
 /* Parent already owns source target/linear mapping holds. This child
  * does not duplicate the binary cold-target marker or release it. Factory
  * integration must retain the graph and retire this child before unbind. */
 in.aperture_base=A04_NATIVE_OUTPUT_VA;in.aperture_bytes=A04_NATIVE_APERTURE_BYTES;
 in.source_va=in.aperture_base+n->target_offset;in.destination_va=in.aperture_base+offset;
 in.source_bytes=a04_native_mdl_extent(n->registered_target);in.destination_bytes=a04_native_mdl_extent(destination);in.shared_va=va[1]+512u;
 in.width=n->width;in.height=n->height;in.pitch_pixels=n->pitch/4u;
 in.destination_width=description->width;in.destination_height=description->height;in.destination_pitch_pixels=description->pitch/4u;
 in.fragment_pds_offset=192;in.secondary_data_offset=256;in.secondary_code_offset=272;
 in.eot_pds_offset=64;in.fragment_usc_offset=128;
 struct geo_copy_region full={0,0,0,0,in.width<in.destination_width?in.width:in.destination_width,in.height<in.destination_height?in.height:in.destination_height};c->input=in;
 if(a04_native_copy_region_job_build(&c->image,&in,&full)||a04_vendor_clear_image_build(&eot,A04_VENDOR_RENDER_SCRATCH_VA,0)){
  status=STATUS_INVALID_PARAMETER;goto fail;
 }
 clear_in.aperture_base=in.aperture_base;clear_in.aperture_bytes=in.aperture_bytes;
 clear_in.target_va=in.source_va;clear_in.target_bytes=in.source_bytes;
 clear_in.width=in.width;clear_in.height=in.height;clear_in.pitch_pixels=in.pitch_pixels;
 /* EOT clear slot0 is disjoint from copy EOT64/fragment192/data256/code272/shared512.
  * Both destinations are fixed from verified resident PTEs before FW starts. */
 if(a04_native_clear_job_build(&c->clear_image,&clear_in)){
  status=STATUS_INVALID_PARAMETER;goto fail;
 }
 for(i=0;i<2;i++){
  /* Parent mappings must point to this native owner's existing table pages.
   * No overwriting an occupied leaf or a different child's directory. */
  if(!r->buffer[dir[i]].cpu||!r->buffer[leaf[i]].cpu||
     a04_rgx_device_read_words(&pc,(UCHAR*)r->buffer[1].cpu+(va[i]>>30)*4u,4u)||
     pc!=((uint32_t)((uint64_t)r->buffer[dir[i]].logical.QuadPart>>8)|1u)||
     a04_rgx_device_read_words(&pd,(UCHAR*)r->buffer[dir[i]].cpu+((va[i]>>21)&511u)*8u,8u)||
     pd!=((uint64_t)r->buffer[leaf[i]].logical.QuadPart|1u)||
     a04_rgx_device_read_words(&entry,r->buffer[leaf[i]].cpu,8u)||entry){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
  status=a04_vendor_uncached_allocate(&c->code[i],r->firmware->adapter,4096u);if(!NT_SUCCESS(status))goto fail;
  status=a04_vendor_native_validate_allocation_cold(n,&c->code[i]);if(!NT_SUCCESS(status)){c->quarantined=TRUE;goto fail;}
  if(allocation_clashes(&c->code[i],c->source)||allocation_clashes(&c->code[i],c->destination)){
   c->quarantined=TRUE;n->quarantined=TRUE;status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;
  }
  if(n->registered_vertex&&allocation_clashes(&c->code[i],n->registered_vertex)){
   c->quarantined=TRUE;n->quarantined=TRUE;status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;
  }
  for(j=0;j<n->linear_count;j++)if(allocation_clashes(&c->code[i],n->linear[j].binding)){
   c->quarantined=TRUE;n->quarantined=TRUE;status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;
  }
  if(i&&(a04_rgx_fw_overlap((uint64_t)c->code[0].logical.QuadPart,4096,(uint64_t)c->code[1].logical.QuadPart,4096)||
    a04_rgx_fw_alias(c->code[0].cpu,4096,c->code[1].cpu,4096))){
   c->quarantined=TRUE;n->quarantined=TRUE;status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;
  }
  /* Newly owned uncached code/data page, never source/destination pixels. */
  _Alignas(8) ULONG zero[16]={0};
  for(j=0;j<4096;j+=sizeof(zero))if(verified((UCHAR*)c->code[i].cpu+j,zero,sizeof(zero))){status=STATUS_IO_DEVICE_ERROR;goto fail;}
 }
 if(verified(c->code[0].cpu,eot.usc,sizeof(eot.usc))||verified((UCHAR*)c->code[0].cpu+128,a04_native_copy_region_usc,sizeof(a04_native_copy_region_usc))||
    verified(c->code[1].cpu,c->clear_image.eot_pds,sizeof(c->clear_image.eot_pds))||
    verified((UCHAR*)c->code[1].cpu+64,c->image.eot_pds,sizeof(c->image.eot_pds))||
    verified((UCHAR*)c->code[1].cpu+192,c->image.fragment_pds,sizeof(c->image.fragment_pds))||
    verified((UCHAR*)c->code[1].cpu+256,c->image.secondary_data,sizeof(c->image.secondary_data))||
    verified((UCHAR*)c->code[1].cpu+272,c->image.secondary_code,sizeof(c->image.secondary_code))||
    verified((UCHAR*)c->code[1].cpu+512,c->image.shared,sizeof(c->image.shared))){status=STATUS_IO_DEVICE_ERROR;goto fail;}
 for(i=0;i<2;i++){
  entry=(uint64_t)c->code[i].logical.QuadPart|A04_GPU_MMU_SLC_BYPASS|1u|(i?0u:A04_GPU_MMU_READ_ONLY);
  c->mapped|=1u<<i; /* Retain child before a potentially partial write. */
  if(verified(r->buffer[leaf[i]].cpu,&entry,8u)){c->quarantined=TRUE;n->quarantined=TRUE;return STATUS_IO_DEVICE_ERROR;}
 }
 c->prepared=TRUE;return STATUS_SUCCESS;
fail:
 (void)a04_native_copy_storage_release_cold(out);return status;
}
const struct a04_native_copy_job_image *a04_native_copy_storage_image(const struct a04_native_copy_storage *c)
{return c&&c->prepared&&!c->quarantined?&c->image:NULL;}
const struct a04_native_copy_job_image *a04_native_copy_storage_clear_image(const struct a04_native_copy_storage *c)
{return c&&c->prepared&&!c->quarantined?&c->clear_image:NULL;}

struct a04_vendor_native_cold *a04_native_copy_storage_parent(const struct a04_native_copy_storage *c)
{return c&&c->prepared&&!c->quarantined?c->native:NULL;}

/* Only the TQ completion path calls this after the actual task, SLC and GP
 * receipts; CPU visibility does not release residency or the submit gate. */
NTSTATUS a04_native_copy_storage_cpu_complete(struct a04_native_copy_storage *c)
{
 NTSTATUS status;
 if(!c||!c->prepared||c->quarantined||!c->native||c->native->quarantined||
    !c->native->render||c->native->render->poisoned||KeGetCurrentIrql()!=PASSIVE_LEVEL)
  return STATUS_INVALID_DEVICE_STATE;
 status=a04_native_mdl_sync_for_cpu(c->source);if(!NT_SUCCESS(status))return status;
 return a04_native_mdl_sync_for_cpu(c->destination);
}
BOOLEAN a04_native_copy_storage_gpu_owned(const struct a04_native_copy_storage *c)
{
 return c&&c->prepared&&!c->quarantined&&
  a04_native_mdl_gpu_owned(c->source)&&a04_native_mdl_gpu_owned(c->destination);
}

/* Factory has fenced the preceding job. Keep the resident WDDM/PTE lease;
 * cache ownership is reacquired at PASSIVE before each TQ publication. No
 * cache operation is issued while already GPU-owned. Ambiguous partial
 * handoff is retained by the caller until the normal Stop/TDR proof. */
NTSTATUS a04_native_copy_storage_gpu_prepare(struct a04_native_copy_storage *c)
{
 NTSTATUS status;
 if(!c||!c->prepared||c->quarantined||!c->native||c->native->quarantined||
    !c->native->render||c->native->render->poisoned||KeGetCurrentIrql()!=PASSIVE_LEVEL)
  return STATUS_INVALID_DEVICE_STATE;
 if(!a04_native_mdl_gpu_owned(c->source)){
  status=a04_native_mdl_sync_for_gpu(c->source);if(!NT_SUCCESS(status))return status;
 }
 if(!a04_native_mdl_gpu_owned(c->destination)){
  status=a04_native_mdl_sync_for_gpu(c->destination);if(!NT_SUCCESS(status))return status;
 }
 return a04_native_copy_storage_gpu_owned(c)?STATUS_SUCCESS:STATUS_INVALID_DEVICE_STATE;
}

NTSTATUS a04_native_copy_storage_region(struct a04_native_copy_storage *c,const struct geo_copy_region *r) {
 struct a04_native_copy_job_image image;
 if(!c||!r||KeGetCurrentIrql()!=PASSIVE_LEVEL||!c->prepared||c->quarantined||c->native->quarantined||
    a04_native_copy_region_job_build(&image,&c->input,r))return STATUS_INVALID_PARAMETER;
 /* Called only by transfer rearm after empty CCB/UFO+completed previous
  * owner boundary. Immutable USC/PTEs never change. Data page is uncached.
  * Any partial data write quarantines the graph and keeps its mapping holds. */
 if(verified((UCHAR*)c->code[1].cpu+512,image.shared,sizeof(image.shared))){
  c->quarantined=TRUE;c->native->quarantined=TRUE;return STATUS_IO_DEVICE_ERROR;
 }
 c->image=image;return STATUS_SUCCESS;
}
