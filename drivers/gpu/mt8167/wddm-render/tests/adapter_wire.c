/* SPDX-License-Identifier: MIT */
#include "../adapter_info.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct geo_adapter_memory_info producer_tuple(void) {
 struct geo_adapter_memory_info p;memset(&p,0,sizeof(p));
 p.size=GEO_ADAPTER_WIRE_BYTES;p.version=GEO_ADAPTER_WIRE_VERSION;p.magic=GEO_ADAPTER_INFO_MAGIC;
 p.owner_ops_version=4;p.control_version=2;p.texture_draw_version=3;p.shader_draw_version=4;
 p.compiler_bvnc=GEO_COMPILER_BVNC;p.draw_version=1;p.format=GEO_FORMAT_A8R8G8B8;p.vertex_max_bytes=GEO_VERTEX_MAX_BYTES;
 return p;
}
int main(void) {
 struct geo_adapter_memory_info good=producer_tuple(),p;unsigned n=0;
 assert(geo_adapter_wire_mismatch(&good)==0);n++;
#define BAD(field,value,bit) do {p=good;p.field=(value);assert(geo_adapter_wire_mismatch(&p)==(bit));n++;}while(0)
 BAD(size,95,GEO_ADAPTER_WIRE_BAD_SIZE);BAD(size,97,GEO_ADAPTER_WIRE_BAD_SIZE);
 BAD(version,7,GEO_ADAPTER_WIRE_BAD_VERSION);BAD(magic,0,GEO_ADAPTER_WIRE_BAD_MAGIC);
 BAD(owner_ops_version,3,GEO_ADAPTER_WIRE_BAD_OWNER);BAD(owner_ops_version,0,GEO_ADAPTER_WIRE_BAD_OWNER);BAD(owner_ops_version,UINT32_MAX,GEO_ADAPTER_WIRE_BAD_OWNER);
 BAD(control_version,1,GEO_ADAPTER_WIRE_BAD_CONTROL);BAD(texture_draw_version,2,GEO_ADAPTER_WIRE_BAD_TEXTURE);BAD(shader_draw_version,3,GEO_ADAPTER_WIRE_BAD_SHADER);
 BAD(compiler_bvnc,good.compiler_bvnc^UINT64_C(0x100000000),GEO_ADAPTER_WIRE_BAD_BVNC);
 BAD(draw_version,0,GEO_ADAPTER_WIRE_BAD_DRAW);BAD(format,0,GEO_ADAPTER_WIRE_BAD_FORMAT);BAD(vertex_max_bytes,65535,GEO_ADAPTER_WIRE_BAD_VERTEX);
 p=good;p.owner_ops_version=3;p.shader_draw_version=0;assert(geo_adapter_wire_mismatch(&p)==(GEO_ADAPTER_WIRE_BAD_OWNER|GEO_ADAPTER_WIRE_BAD_SHADER));n++;
 /* Capacity and aperture contents remain live query data, not invented
  * expected constants in the fixed-tuple validator. */
 p=good;p.max_allocation_bytes=123;p.aperture.gpu_base=UINT64_C(0x12345000);assert(geo_adapter_wire_mismatch(&p)==0);n++;
 printf("ADAPTER_WIRE_BOUNDARIES_PASS %u\n",n);return 0;
}
