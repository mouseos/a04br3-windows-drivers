/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "native_hwrt_profile.h"
#include "native_d3d_viewport.h"
#include "native_copy_job.h"
#include "../drivers/gpu/mt8167/wddm-render/render_layout.h"
#include "../drivers/gpu/mt8167/wddm-render/viewport_profile.h"
static uint32_t bits(float f){uint32_t b;memcpy(&b,&f,4);return b;}
static float number(uint32_t b){float f;memcpy(&f,&b,4);return f;}
int main(void){
 struct a04_m111_target_storage ts;struct a04_m111_mlist_storage ml;
 _Alignas(8) struct a04_m111_hwrt_input hw[2];
 assert(!a04_m111_layout_pm_mlists(0x100000,0x65c0300000ULL,8192,&ml));
 unsigned accepted=0;
 for(unsigned w=1;w<=4096;w++){
  unsigned h=1+(w*127u)%4096u,pitch=geo_render_pitch(w);
  if(!geo_render_layout_valid(w,h,pitch))continue;
  assert(!a04_m111_layout_target_storage(w,h,0x65c0000000ULL,0x100000,0x65c0100000ULL,0x100000,&ts));
  assert(!a04_m111_build_single_rt_hwrt_profile(hw,w,h,&ts,&ml));
  assert(hw[0].merge_lower_x==bits(number(0x3e89309c)/(float)w));
  assert(hw[0].merge_upper_y==bits(number(0x3fddb3d9)/(float)h));
  assert(hw[0].merge_scale_x==bits((float)(w*16)/number(0x3fbb67b2)));
  assert(hw[0].merge_scale_y==bits((float)(h*16)/number(0x3fbb67b2)));
  uint32_t vp[6]={0,0,bits((float)w),bits((float)h),0,0x3f800000},out[6];
  assert(geo_viewport_target_valid(vp,w,h));assert(!a04_m111_build_d3d_viewport(out,vp));
  assert(out[0]==bits(w/2.0f)&&out[1]==bits(w/2.0f));
  assert(out[2]==bits(h/2.0f)&&out[3]==bits(-(float)h/2.0f));accepted++;
 }
 assert(geo_render_layout_valid(1280,800,5120));assert(geo_render_layout_valid(801,600,3328));
 assert(!geo_render_layout_valid(1280,801,5120));assert(!geo_render_layout_valid(801,600,3204));
 _Alignas(8) struct a04_native_copy_job_input q={0};
 q.aperture_base=q.source_va=0x8200000000ULL;q.aperture_bytes=0x1000000;
 q.destination_va=q.source_va+0x400000;q.source_bytes=4096000;q.destination_bytes=1331200;
 q.shared_va=0xa800000200ULL;q.width=1280;q.height=800;q.pitch_pixels=1280;
 q.destination_width=801;q.destination_height=400;q.destination_pitch_pixels=832;
 q.fragment_pds_offset=192;q.secondary_data_offset=256;q.secondary_code_offset=272;q.eot_pds_offset=64;q.fragment_usc_offset=128;
 struct geo_copy_region r={17,19,23,29,311,173};struct a04_native_copy_job_image image,old;
 assert(!a04_native_copy_region_job_build(&image,&q,&r));
 assert(image.task[6]==((51u<<16)|25u));
 assert(image.task[34]==201u&&image.task[35]==333u+(29u<<14));
 assert(image.shared[4]!=0);old=image;r.width=800;
 assert(a04_native_copy_region_job_build(&image,&q,&r)&&!memcmp(&old,&image,sizeof(old)));
 r.width=311;q.source_bytes=4095999;
 assert(a04_native_copy_region_job_build(&image,&q,&r)&&!memcmp(&old,&image,sizeof(old)));
 printf("geometry/copy production producers: %u layouts; float oracle, viewport, independent pitches, extent rejection PASS; hardware unverified\n",accepted);
}
