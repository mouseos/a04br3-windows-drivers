/* SPDX-License-Identifier: MIT. Independent readback oracle, never GPU input.
 * Exact integer pixel-center test for the non-edge-aligned right triangles
 * produced by the four documented diagonal+translation CB transforms. */
#ifndef GEO_MATRIX_ORACLE_H
#define GEO_MATRIX_ORACLE_H
#include <stdint.h>
struct geo_matrix_case {float rows[16];uint32_t left,top,width,height;};
static const struct geo_matrix_case geo_matrix_cases[]={
 {{.5f,0,0,0,0,.5f,0,0,0,0,1,0,0,0,0,1},200,150,400,300},
 {{.5f,0,0,.25f,0,.5f,0,0,0,0,1,0,0,0,0,1},300,150,400,300},
 {{.25f,0,0,0,0,.5f,0,.25f,0,0,1,0,0,0,0,1},300,75,200,300},
 {{.5f,0,0,-.25f,0,.25f,0,-.25f,0,0,1,0,0,0,0,1},100,300,400,150}
};
static int geo_matrix_pixel_inside(const struct geo_matrix_case *c,uint32_t x,uint32_t y) {
 if(x<c->left||y<c->top||x>=c->left+c->width||y>=c->top+c->height)return 0;
 return ((uint64_t)(2u*(x-c->left)+1)*c->height+(uint64_t)(2u*(y-c->top)+1)*c->width)<2u*(uint64_t)c->width*c->height;
}
#endif
