/* SPDX-License-Identifier: MIT */
#include "native_color_transport.h"
static int ready(const struct a04_color_transport *t)
{
 return t && t->draw.size==sizeof(t->draw) && t->draw.version==2 && t->draw.context && t->draw.prepare_draw &&
 t->owner.size==sizeof(t->owner) && t->owner.version==GEO_OWNER_OPS_VERSION &&
 t->owner.context==t->draw.context && t->owner.submit && t->owner.discard_prepared &&
 (t->owner.capabilities&GEO_OWNER_REQUIRED)==GEO_OWNER_REQUIRED;
}
int32_t a04_native_color_transport_bind(struct a04_color_transport *out,
 const struct geo_native_draw_ops_v2 *draw,const struct geo_owner_ops *owner)
{
 if(!out || !draw || !owner)return GEO_INVALID;
 struct a04_color_transport made={*draw,*owner};
 if(!ready(&made))return GEO_NOT_READY;
 *out=made;return GEO_OK;
}
int32_t a04_native_color_transport_prepare(const struct a04_color_transport *t,
 const void *capture,uint32_t bytes,uint32_t count,uint64_t target,uint64_t vertex,uint64_t *job)
{
 const struct a04_color_packet *p=capture;struct geo_draw_request_v2 r;
 if(!job)return GEO_INVALID;
 *job=0;
 if(!ready(t))return GEO_NOT_READY;
 if(!p || ((uintptr_t)p&7) || bytes<sizeof(*p) || bytes-sizeof(*p)>GEO_DRAW_MAX_BLOB ||
    p->magic!=A04_COLOR_PACKET_MAGIC || p->version!=2 || p->size!=bytes || p->element_count!=2 ||
    p->reserved[0] || p->reserved[1] || p->target_index>=count || p->vertex_index>=count ||
    p->target_index==p->vertex_index || !target || !vertex || target==vertex ||
    p->request.base.target || p->request.base.vertex)return GEO_INVALID;
 for(unsigned i=0;i<2;i++){
  const struct a04_color_element *e=&p->elements[i];
  if(e->input_slot || e->input_register!=i || e->byte_offset!=16*i || e->components!=4 ||
     e->bit_size!=32 || e->step_rate)return GEO_UNSUPPORTED;
 }
 r=p->request;r.base.target=target;r.base.vertex=vertex;
 if(r.base.state.vertex_count!=3 || r.base.state.vertex_stride!=32 ||
    r.base.state.first_vertex || r.base.state.vertex_offset)return GEO_UNSUPPORTED;
 int32_t status=geo_draw_validate_capture_v2(&r,p+1,bytes-(uint32_t)sizeof(*p));if(status)return status;
 return t->draw.prepare_draw(t->draw.context,&r,p+1,bytes-(uint32_t)sizeof(*p),job);
}
int32_t a04_native_color_transport_submit(const struct a04_color_transport *t,
 uint64_t job,uint32_t fence,uint32_t generation)
{
 if(!ready(t))return GEO_NOT_READY;
 if(!job)return GEO_INVALID;
 return t->owner.submit(t->owner.context,job,fence,generation);
}
