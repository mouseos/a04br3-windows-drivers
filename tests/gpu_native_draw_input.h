/* SPDX-License-Identifier: MIT. Actual H63 source-generated program fixture. */
#ifndef A04_TEST_NATIVE_DRAW_INPUT_H
#define A04_TEST_NATIVE_DRAW_INPUT_H
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "native_draw_job.h"
static void put(uint8_t *p,unsigned at,uint32_t v)
{for(unsigned i=0;i<4;i++)p[at+i]=(uint8_t)(v>>(i*8));}
static struct geo_draw_request draw_input(const char *path,uint8_t blob[1400])
{
 FILE *f=fopen(path,"r");assert(f);char line[1024];unsigned found=0;uint8_t code[2][64]={{0}};
 while(fgets(line,sizeof(line),f)){
  size_t n=strcspn(line,"\r\n");if(n!=48 && n!=128)continue;
  unsigned which=n==48?0:1;for(size_t i=0;i<n/2;i++){unsigned v;assert(sscanf(line+2*i,"%2x",&v)==1);code[which][i]=(uint8_t)v;}found|=1u<<which;
 }
 fclose(f);assert(found==3);
 memset(blob,0,1400);struct geo_draw_request r={0};r.size=sizeof(r);r.version=1;r.blob_bytes=1400;r.target=1;r.vertex=2;
 r.target_desc=(struct geo_surface_desc){sizeof(r.target_desc),1,800,600,3200,21,1,0};
 r.state.topology=1;r.state.vertex_count=3;r.state.vertex_stride=16;r.state.color_write_mask=15;
 r.state.viewport[2]=0x44480000;r.state.viewport[3]=0x44160000;r.state.viewport[5]=0x3f800000;
 r.vs=(struct geo_draw_shader){.format=2,.stage=0,.tokens={0,12},.usc={16,24},.constants={128,512},.vtxins=4,.vtxouts=4,.input_mask=1};
 r.ps=(struct geo_draw_shader){.format=2,.stage=1,.tokens={40,12},.usc={56,64},.constants={640,512},.temps=5,.shareds=4,.constant_mask=1};
 /* Exact official token inputs corresponding to these USC bytes. */
 put(blob,1216,0x10040u);put(blob,1220,0xfu);put(blob,1224,0x300005fu);put(blob,1228,0x1010f2u);put(blob,1232,0x0u);put(blob,1236,0x4000067u);put(blob,1240,0x1020f2u);put(blob,1244,0x0u);put(blob,1248,0x1u);put(blob,1252,0x5000036u);put(blob,1256,0x1020f2u);put(blob,1260,0x0u);put(blob,1264,0x101e46u);put(blob,1268,0x0u);put(blob,1272,0x100003eu);
 r.vs.tokens=(struct geo_draw_span){1216,60};
 put(blob,1152,0x40u);put(blob,1156,0x10u);put(blob,1160,0x4000059u);put(blob,1164,0x208e46u);put(blob,1168,0x0u);put(blob,1172,0x1u);put(blob,1176,0x3000065u);put(blob,1180,0x1020f2u);put(blob,1184,0x0u);put(blob,1188,0x6000036u);put(blob,1192,0x1020f2u);put(blob,1196,0x0u);put(blob,1200,0x208e46u);put(blob,1204,0x0u);put(blob,1208,0x0u);put(blob,1212,0x100003eu);
 r.ps.tokens=(struct geo_draw_span){1152,64};

 memcpy(blob+16,code[0],24);memcpy(blob+56,code[1],64);put(blob,640,0x3f800000);put(blob,652,0x3f800000);
 return r;
}
#endif
