/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "native_copy_job.h"
#include "native_copy_state.h"
#include "clear_image.h"
#include "render_resources.h"
static int bad(struct a04_native_copy_job_image *p,struct a04_native_copy_job_input *q)
{struct a04_native_copy_job_image old=*p;int r=a04_native_copy_job_build(p,q);assert(!memcmp(&old,p,sizeof(old)));return r;}
int main(void)
{
 _Alignas(8) struct a04_native_copy_job_input q={UINT64_C(0x8200000000),4194304u,
 UINT64_C(0x8200000000),UINT64_C(0x8200200000),1921024u,1921024u,
 UINT64_C(0x65c0002000),800,600,800,192,256,272,64,128};
 struct a04_native_copy_job_image image,old;
 struct a04_vendor_wide_state wide;
 assert(!a04_native_copy_job_build(&image,&q));
 assert(!a04_vendor_copy_wide_state_build(&wide,q.source_va,800,600,800));
 assert(!memcmp(image.shared,wide.shared,sizeof(wide.shared)));
 assert(!memcmp(image.task+32,wide.csr,sizeof(wide.csr)));
 assert(image.task[6]==((50u<<16)|38u));
 assert(image.task[34]==599u&&image.task[35]==799u); /* nonsquare clip */
 assert(image.eot_pds[2]==(uint32_t)(q.destination_va>>2));
 assert(image.eot_pds[3]==((uint32_t)(q.destination_va>>34)|(1u<<19)));
 assert(image.secondary_data[0]==(uint32_t)q.shared_va);
 assert(image.secondary_data[1]==(uint32_t)(q.shared_va>>32));
 old=image;
 q.destination_va=q.source_va;assert(bad(&image,&q));
 q.destination_va=UINT64_C(0x8200200000);q.source_bytes=1919999;assert(bad(&image,&q));
 q.source_bytes=1921024;q.pitch_pixels=801;assert(bad(&image,&q));
 q.pitch_pixels=800;q.shared_va=q.source_va;assert(bad(&image,&q));
 q.shared_va=UINT64_C(0x65c0002000);q.eot_pds_offset=192;assert(bad(&image,&q));
 q.eot_pds_offset=64;q.fragment_usc_offset=65536-148;assert(bad(&image,&q));
 q.fragment_usc_offset=128;q.aperture_base=UINT64_MAX-4095u;assert(bad(&image,&q));
 q.aperture_base=UINT64_C(0x8200000000);q.width=32;q.height=32;q.pitch_pixels=32;
 q.source_bytes=q.destination_bytes=4096;
 assert(!a04_native_copy_job_build(&image,&q));
 /* Match all 52 legacy direct-copy words, not merely the new geometry. */
 struct a04_vendor_copy_kick_input ki={192,256,272,128};
 _Alignas(8) struct a04_vendor_copy_kick_image k;
 _Alignas(8) struct a04_vendor_clear_image eot;
 uint32_t task[52]={0};
 assert(!a04_vendor_copy_kick_build(&k,&ki));assert(!a04_vendor_clear_image_build(&eot,A04_VENDOR_RENDER_SCRATCH_VA,0));
 task[0]=0x200;task[1]=eot.payload[1];task[6]=eot.payload[6];task[8]=eot.payload[8];
 task[10]=eot.payload[10];task[11]=64+eot.payload[11];task[12]=64+eot.payload[12];
 #define PUT(off,x) do{uint64_t v=(x);task[(off)/4]=(uint32_t)v;task[(off)/4+1]=(uint32_t)(v>>32);}while(0)
 PUT(0x38,k.raw38);PUT(0x40,k.raw40);PUT(0x48,k.raw48);PUT(0x50,k.raw50);PUT(0x58,k.raw58);PUT(0x60,3u);
 #undef PUT
 memcpy(task+32,eot.payload+32,16);assert(!memcmp(task,image.task,sizeof(task)));
 assert(memcmp(old.task,image.task,sizeof(task)));
 /* Fast-fill golden packet, then the same nonsquare resident RT geometry
  * used by the production Clear -> Copy -> Clear sequence. */
 _Alignas(8) struct a04_native_clear_job_input fill={A04_VENDOR_RENDER_SCRATCH_VA,
  4096,A04_VENDOR_RENDER_SCRATCH_VA,4096,32,32,32,0,0xffff00ffu};
 assert(!a04_native_clear_job_build(&image,&fill));
 assert(!a04_vendor_clear_image_build(&eot,A04_VENDOR_RENDER_SCRATCH_VA,fill.colour));
 assert(!memcmp(image.task,eot.payload,sizeof(eot.payload)));
 assert(!memcmp(image.eot_pds,eot.pds,sizeof(eot.pds)));
 fill.aperture_base=fill.target_va=UINT64_C(0x8200000000);fill.aperture_bytes=4194304;
 fill.target_bytes=1921024;fill.width=800;fill.height=600;fill.pitch_pixels=800;
 fill.colour=0xa5a5a5a5u;assert(!a04_native_clear_job_build(&image,&fill));
 assert(image.task[2]==fill.colour&&image.task[6]==((50u<<16)|38u));
 assert(image.task[34]==599u&&image.task[35]==799u);
 old=image;fill.target_bytes=1919999;assert(a04_native_clear_job_build(&image,&fill));
 assert(!memcmp(&old,&image,sizeof(old)));return 0;
}
