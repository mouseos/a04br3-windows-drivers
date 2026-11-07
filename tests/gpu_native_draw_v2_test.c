/* SPDX-License-Identifier: MIT. CPU producer, not hardware execution. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu_native_draw_v2_input.h"
static uint64_t get(const uint8_t *p,unsigned at,unsigned n)
{uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[at+i]<<(8*i);return v;}
int main(int argc,char **argv)
{
 assert(argc==2);_Alignas(8) uint8_t blob[2048];struct geo_draw_request_v2 r=draw_v2_input(argv[1],blob);
 struct a04_m111_draw_job *out=malloc(sizeof(*out)),*old=malloc(sizeof(*out));
 void *scratch=aligned_alloc(8,(A04_M111_DRAW_SCRATCH+7)&~7u);assert(out&&old&&scratch);
 struct a04_m111_clear_job_input in={.output_gpu=0x8200000000ULL,.data_page_gpu=0x65c0100000ULL,.usc_page_gpu=0xba00010000ULL,.pds_page_gpu=0xa800010000ULL,.dummy_region_gpu=0x65c0020000ULL,.dummy_stencil_gpu=0x65c0030000ULL,.hwrt_fw=0xc00848c0,.pr_fence_fw=0xc0084c00};
 for(unsigned k=0;k<2;k++) {
  uint64_t vertex=0x82003ff000ULL;
  assert(!a04_m111_build_draw_job_v2(out,&in,vertex,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH));
  assert(!memcmp(out->usc+0x80,blob+16,88) && !memcmp(out->usc+0x100,blob+112,80));
  assert(get(out->pds,0x148,4)==32 && get(out->pds,0x150,8)==vertex && get(out->pds,0x158,4)==0x80000008u);
  assert(get(out->data,48,4)==8 && ((get(out->data,52,4)>>10)&63)==2);
  assert(get(out->data,0x240+7*4,4)==(0x400u|0x20u|1u));
  assert(get(out->data,0x240+8*4,4)==0x4000u); /* no constants/shareds */
  assert(get(out->data,0x240+9*4,4)==in.pds_page_gpu-0xa800000000ULL+0x380);
  assert(get(out->data,0x240+20*4,4)==0x08010000u && get(out->data,0x240+21*4,4)==4);
  assert(get(out->data,0x27c,4)==0xc3960000u && get(out->data,0x284,4)==0x3f800000u && !get(out->data,0x288,4));
  assert(get(out->pds,0x380,8)==0x03000000ULL); /* W -> coeff0 */
  assert(get(out->pds,0x388,8)==(0x8000000000000000ULL|(1ULL<<54)|0x030d0202)); /* COLOR -> coeff4 */
  assert(get(out->pds,0x390,4)==0xf0000004u && get(out->pds,0x394,4)==0xf4000104u);
  assert((get(out->pds,0x180,8)&0xffffffffu)==in.usc_page_gpu+0x100-0xba00000000ULL);
  memcpy(old,out,sizeof(*out));
  r.link.fs_coeffs=16;assert(a04_m111_build_draw_job_v2(out,&in,vertex,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)==GEO_UNSUPPORTED);r.link.fs_coeffs=20;
  blob[112]^=1;assert(a04_m111_build_draw_job_v2(out,&in,vertex,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)==GEO_UNSUPPORTED);blob[112]^=1;
  r.link.vs_entry_offset=4;assert(a04_m111_build_draw_job_v2(out,&in,vertex,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)==GEO_UNSUPPORTED);r.link.vs_entry_offset=0;
  assert(!memcmp(old,out,sizeof(*out)));
  in.output_gpu+=0x100000;in.data_page_gpu+=0x200000;in.usc_page_gpu+=0x1000;in.pds_page_gpu+=0x2000;
 }
 free(out);free(old);free(scratch);
 puts("linked v2: real source pair+official tokens, complete TA/3D/VDM/PPP/PDS, dynamic heap relocation and atomic rejection PASS; not GPU");return 0;
}
