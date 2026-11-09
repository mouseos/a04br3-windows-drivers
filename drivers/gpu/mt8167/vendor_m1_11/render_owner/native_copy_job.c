/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_copy_job.h"
#include "display_geometry356.h"
#include "native_copy_state.h"
#include "clear_image.h"
#include "render_resources.h"
#include <string.h>
int a04_native_clear_job_build(struct a04_native_copy_job_image *out,
 const struct a04_native_clear_job_input *request)
{
 struct a04_native_copy_job_image image={0};
 struct a04_native_clear_job_input q;
 struct a04_vendor_wide_state wide;
 _Alignas(8) struct a04_vendor_clear_image fill;
 uint64_t extent;uintptr_t a=(uintptr_t)out,b=(uintptr_t)request;
 if(!out||!request||(a&3u)||(b&7u)||a>UINTPTR_MAX-sizeof(*out)||
    b>UINTPTR_MAX-sizeof(*request)||(a<b+sizeof(*request)&&b<a+sizeof(*out)))return 1;
 q=*request;
 if(!q.width||!q.height||q.width>800u||q.height>A04_NATIVE_FRAME_HEIGHT||q.pitch_pixels<q.width||
    q.pitch_pixels>1024u||(q.pitch_pixels&31u)||!q.aperture_bytes||
    q.aperture_bytes>A04_NATIVE_APERTURE_BYTES||(q.aperture_base&4095u)||(q.aperture_bytes&4095u)||
    q.aperture_base>=(UINT64_C(1)<<40)||q.aperture_bytes>(UINT64_C(1)<<40)-q.aperture_base||
    (q.target_va&127u)||q.target_va<q.aperture_base||
    q.target_va-q.aperture_base>q.aperture_bytes||
    q.target_bytes>q.aperture_bytes-(q.target_va-q.aperture_base)||
    (q.pds_offset&63u)||q.pds_offset>65536u-64u)return 1;
 extent=(uint64_t)q.pitch_pixels*q.height*4u;
 if(q.target_bytes<extent||
    a04_vendor_copy_wide_state_build(&wide,q.target_va,q.width,q.height,q.pitch_pixels)||
    a04_vendor_clear_image_build(&fill,A04_VENDOR_RENDER_SCRATCH_VA,0))return 1;
 /* Same native22 transfer-fill packet and EOT as clear_tile. General
  * pitch, clipping and tile counts come from the proven DOD wide producer.
  * Colour lives in command data; even the diagnostic sentinel is a valid
  * D3D clear value. No executable/PTE or destination pixel is modified. */
 memcpy(image.task,fill.payload,sizeof(fill.payload));
 image.task[2]=q.colour;image.task[6]=wide.tile_size;
 image.task[11]+=q.pds_offset;image.task[12]+=q.pds_offset;
 memcpy(image.task+32,wide.csr,sizeof(wide.csr));
 memcpy(image.eot_pds,fill.pds,sizeof(fill.pds));
 image.eot_pds[2]=(uint32_t)(q.target_va>>2);
 image.eot_pds[3]=(uint32_t)(q.target_va>>34)|(1u<<19);
 *out=image;return 0;
}
static int overlap(uint64_t a,uint64_t an,uint64_t b,uint64_t bn)
{return a<=b?b-a<an:a-b<bn;}
static int span(uint64_t base,uint64_t bytes,uint64_t at,uint64_t n)
{return at>=base&&at-base<=bytes&&n<=bytes-(at-base);}
int a04_native_copy_job_build(struct a04_native_copy_job_image *out,
 const struct a04_native_copy_job_input *request)
{
 struct a04_native_copy_job_input q;
 struct a04_native_copy_job_image image={0};
 struct a04_vendor_wide_state wide;
 struct a04_vendor_copy_kick_input ki;
 _Alignas(8) struct a04_vendor_copy_kick_image kick;
 _Alignas(8) struct a04_vendor_clear_image eot;
 struct a04_vendor_copy_state_input si;
 struct a04_vendor_copy_state secondary;
 uint64_t extent;uintptr_t a=(uintptr_t)out,b=(uintptr_t)request;
 if(!out||!request||(a&3u)||(b&7u)||a>UINTPTR_MAX-sizeof(*out)||b>UINTPTR_MAX-sizeof(*request)||
    overlap(a,sizeof(*out),b,sizeof(*request)))return 1;
 q=*request;
 /* Bounds include the final padded row. The registered allocation extent,
  * not a naked VA, must contain all sampled and written bytes. */
 if(!q.width||!q.height||q.width>800u||q.height>A04_NATIVE_FRAME_HEIGHT||q.pitch_pixels<q.width||
    q.pitch_pixels>1024u||(q.pitch_pixels&31u)||!q.aperture_bytes||q.aperture_bytes>A04_NATIVE_APERTURE_BYTES||
    (q.aperture_base&4095u)||(q.aperture_bytes&4095u)||q.aperture_base>=(UINT64_C(1)<<40)||
    q.aperture_bytes>(UINT64_C(1)<<40)-q.aperture_base||
    (q.source_va&127u)||(q.destination_va&127u)||(q.shared_va&15u))return 1;
 extent=(uint64_t)q.pitch_pixels*q.height*4u;
 if(q.source_bytes<extent||q.destination_bytes<extent||
    !span(q.aperture_base,q.aperture_bytes,q.source_va,q.source_bytes)||
    !span(q.aperture_base,q.aperture_bytes,q.destination_va,q.destination_bytes)||
    overlap(q.source_va,q.source_bytes,q.destination_va,q.destination_bytes)||
    q.shared_va>=(UINT64_C(1)<<40)||36u>(UINT64_C(1)<<40)-q.shared_va||
    overlap(q.shared_va,36,q.aperture_base,q.aperture_bytes))return 1;
 /* The legacy producer fixes source/destination to private scratch. Use it
  * only to construct its invariant secondary program, then bind the actual
  * shared address; texture/PBE addresses come from the validated wide path. */
 si=(struct a04_vendor_copy_state_input){A04_VENDOR_RENDER_SCRATCH_VA,
  A04_VENDOR_RENDER_SCRATCH_VA+4096u,A04_VENDOR_RENDER_SCRATCH_VA+8192u};
 if(a04_vendor_copy_state_build(&secondary,&si)||
    a04_vendor_copy_wide_state_build(&wide,q.source_va,q.width,q.height,q.pitch_pixels))return 1;
 ki=(struct a04_vendor_copy_kick_input){q.fragment_pds_offset,q.secondary_data_offset,
  q.secondary_code_offset,q.fragment_usc_offset};
 if(a04_vendor_copy_kick_build(&kick,&ki)||
    a04_vendor_clear_image_build(&eot,A04_VENDOR_RENDER_SCRATCH_VA,0)||
    (q.eot_pds_offset&15u)||q.eot_pds_offset>65536u-64u||
    overlap(q.eot_pds_offset,64,q.fragment_pds_offset,64)||
    overlap(q.eot_pds_offset,64,q.secondary_data_offset,16)||
    overlap(q.eot_pds_offset,64,q.secondary_code_offset,16))return 1;
 /* Same complete zero-initialized direct-copy record as scratch snapshot:
  * no ISP list, no optional backing, one output. Nonzero words are copied
  * from existing producers, including native22 format and clipping order. */
 image.task[0]=kick.raw00;image.task[1]=eot.payload[1];
 image.task[6]=wide.tile_size;image.task[8]=eot.payload[8];
 image.task[10]=eot.payload[10];
 image.task[11]=q.eot_pds_offset+eot.payload[11];
 image.task[12]=q.eot_pds_offset+eot.payload[12];
 #define QWORD(off,value) do {uint64_t v=(value);image.task[(off)/4u]=(uint32_t)v;image.task[(off)/4u+1]=(uint32_t)(v>>32);} while(0)
 QWORD(0x38,kick.raw38);QWORD(0x40,kick.raw40);QWORD(0x48,kick.raw48);
 QWORD(0x50,kick.raw50);QWORD(0x58,kick.raw58);QWORD(0x60,3u);
 #undef QWORD
 memcpy(image.task+32,wide.csr,sizeof(wide.csr));
 memcpy(image.shared,wide.shared,sizeof(wide.shared));
 memcpy(image.fragment_pds,kick.pds_fragment,sizeof(kick.pds_fragment));
 memcpy(image.secondary_data,secondary.pds_data,sizeof(secondary.pds_data));
 image.secondary_data[0]=(uint32_t)q.shared_va;
 image.secondary_data[1]=(uint32_t)(q.shared_va>>32);
 memcpy(image.secondary_code,secondary.pds_code,sizeof(secondary.pds_code));
 memcpy(image.eot_pds,eot.pds,sizeof(eot.pds));
 image.eot_pds[2]=(uint32_t)(q.destination_va>>2);
 image.eot_pds[3]=(uint32_t)(q.destination_va>>34)|(1u<<19);
 *out=image;return 0;
}
