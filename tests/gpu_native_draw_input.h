/* SPDX-License-Identifier: MIT. Actual H63 source-generated program fixture. */
#ifndef A04_TEST_NATIVE_DRAW_INPUT_H
#define A04_TEST_NATIVE_DRAW_INPUT_H
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "native_draw_job.h"
static void put(uint8_t *p,unsigned at,uint32_t v)
{for(unsigned i=0;i<4;i++)p[at+i]=(uint8_t)(v>>(i*8));}
static struct geo_draw_request draw_input(const char *path,uint8_t blob[1200])
{
 FILE *f=fopen(path,"r");assert(f);char line[1024];unsigned found=0;uint8_t code[2][64]={{0}};
 while(fgets(line,sizeof(line),f)){
  size_t n=strcspn(line,"\r\n");if(n!=48 && n!=128)continue;
  unsigned which=n==48?0:1;for(size_t i=0;i<n/2;i++){unsigned v;assert(sscanf(line+2*i,"%2x",&v)==1);code[which][i]=(uint8_t)v;}found|=1u<<which;
 }
 fclose(f);assert(found==3);
 memset(blob,0,1200);struct geo_draw_request r={0};r.size=sizeof(r);r.version=1;r.blob_bytes=1200;r.target=1;r.vertex=2;
 r.target_desc=(struct geo_surface_desc){sizeof(r.target_desc),1,800,600,3200,21,1,0};
 r.state.topology=1;r.state.vertex_count=3;r.state.vertex_stride=16;r.state.color_write_mask=15;
 r.state.viewport[2]=0x44480000;r.state.viewport[3]=0x44160000;r.state.viewport[5]=0x3f800000;
 r.vs=(struct geo_draw_shader){.format=2,.stage=0,.tokens={0,12},.usc={16,24},.constants={128,512},.vtxins=4,.vtxouts=4,.input_mask=1};
 r.ps=(struct geo_draw_shader){.format=2,.stage=1,.tokens={40,12},.usc={56,64},.constants={640,512},.temps=5,.shareds=4,.constant_mask=1};
 put(blob,0,0x10040);put(blob,4,3);put(blob,8,0x0100003e);put(blob,40,0x40);put(blob,44,3);put(blob,48,0x0100003e);
 memcpy(blob+16,code[0],24);memcpy(blob+56,code[1],64);put(blob,640,0x3f800000);put(blob,652,0x3f800000);
 return r;
}
#endif
