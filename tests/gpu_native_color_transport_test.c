/* SPDX-License-Identifier: MIT. Transport-call doubles; no GPU success. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/native_color_transport.h"
#include "gpu_native_draw_v2_input.h"
static unsigned prepared,submitted;static int context;
static int32_t prepare(void *c,const struct geo_draw_request_v2 *r,const void *b,uint32_t n,uint64_t *j)
{assert(c==&context&&r->base.target==11&&r->base.vertex==22&&b&&n==2048);*j=0;prepared++;return GEO_BUSY;}
static int32_t discard(void *c,uint64_t j){(void)c;(void)j;return GEO_BUSY;}
static int32_t submit(void *c,uint64_t j,uint32_t f,uint32_t g)
{assert(c==&context&&j==123&&f==0&&g==7);submitted++;return GEO_BUSY;}
int main(int argc,char **argv)
{
 assert(argc==2);unsigned n=sizeof(struct a04_color_packet)+2048;
 struct a04_color_packet *p=calloc(1,n);assert(p);
 struct geo_draw_request_v2 r=draw_v2_input(argv[1],(uint8_t *)(p+1));
 p->magic=A04_COLOR_PACKET_MAGIC;p->size=n;p->version=2;p->element_count=2;p->vertex_index=1;p->request=r;
 p->request.base.target=p->request.base.vertex=0;
 for(unsigned i=0;i<2;i++)p->elements[i]=(struct a04_color_element){0,i,i*16,4,32,0};
 struct geo_native_draw_ops_v2 d={sizeof(d),2,&context,prepare};
 struct geo_owner_ops o={.size=sizeof(o),.version=2,.capabilities=GEO_OWNER_REQUIRED,.context=&context,.discard_prepared=discard,.submit=submit};
 struct a04_color_transport t={0};uint64_t job=9;
 assert(!a04_native_color_transport_bind(&t,&d,&o));
 assert(a04_native_color_transport_prepare(&t,p,n,2,11,22,&job)==GEO_BUSY&&!job&&prepared==1);
 p->elements[1].byte_offset=12;
 assert(a04_native_color_transport_prepare(&t,p,n,2,11,22,&job)==GEO_UNSUPPORTED&&prepared==1);
 p->elements[1].byte_offset=16;p->request.base.target=11;
 assert(a04_native_color_transport_prepare(&t,p,n,2,11,22,&job)==GEO_INVALID&&prepared==1);
 assert(a04_native_color_transport_submit(&t,123,0,7)==GEO_BUSY&&submitted==1);
 d.context=p;assert(a04_native_color_transport_bind(&t,&d,&o)==GEO_NOT_READY);
 free(p);puts("COLOR V2 transport:2 elements/96bytes, resolved IDs, actual callback delegation/error propagation, fence0 PASS; no GPU success");return 0;
}
