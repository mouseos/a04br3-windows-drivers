/* SPDX-License-Identifier: MIT. Inputs are explicit source compiler outputs. */
#ifndef A04_TEST_DRAW_V2_INPUT_H
#define A04_TEST_DRAW_V2_INPUT_H
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_draw_job.h"
static unsigned read_input(const char *directory,const char *name,void *out,unsigned cap)
{
 char path[4096];assert(snprintf(path,sizeof(path),"%s/%s",directory,name)<(int)sizeof(path));
 FILE *f=fopen(path,"rb");assert(f);unsigned n=(unsigned)fread(out,1,cap,f);assert(!ferror(f)&&feof(f));fclose(f);return n;
}
static struct geo_draw_request_v2 draw_v2_input(const char *directory,uint8_t blob[2048])
{
 memset(blob,0,2048);struct geo_draw_request_v2 r={0};struct geo_draw_request *b=&r.base;
 b->size=sizeof(r);b->version=2;b->blob_bytes=2048;b->target=1;b->vertex=2;
 b->target_desc=(struct geo_surface_desc){sizeof(b->target_desc),1,800,600,3200,21,1,0};
 b->state.topology=1;b->state.vertex_count=3;b->state.vertex_stride=32;b->state.color_write_mask=15;
 b->state.viewport[2]=0x44480000;b->state.viewport[3]=0x44160000;b->state.viewport[5]=0x3f800000;
 b->vs=(struct geo_draw_shader){.format=2,.stage=0,.usc={16,0},.tokens={1280,0},.constants={256,512},.vtxins=8,.vtxouts=8,.input_mask=3};
 b->ps=(struct geo_draw_shader){.format=2,.stage=1,.usc={112,0},.tokens={1536,0},.constants={768,512},.temps=5,.input_mask=1};
 b->vs.usc.bytes=read_input(directory,"vs.usc",blob+16,96);b->ps.usc.bytes=read_input(directory,"fs.usc",blob+112,144);
 b->vs.tokens.bytes=read_input(directory,"vs.tokens.bin",blob+1280,256);b->ps.tokens.bytes=read_input(directory,"fs.tokens.bin",blob+1536,256);
 r.link.size=sizeof(r.link);r.link.version=2;r.link.varying_count=1;r.link.vs_vtxouts=8;r.link.vs_f32_smooth=4;
 r.link.position.count=4;r.link.fs_coeffs=20;r.link.fs_uses_w=1;
 r.link.varying[0]=(struct geo_draw_link_varying){GEO_DRAW_SEMANTIC_COLOR,0,4,32,GEO_DRAW_INTERP_SMOOTH,{4,4,0},{4,16,0}};
 return r;
}
#endif
