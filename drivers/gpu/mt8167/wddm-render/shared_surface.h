/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_SHARED_SURFACE_H
#define GEO_SHARED_SURFACE_H
#include "clear_protocol.h"
#include "texture_layout.h"
/* Pointer-free WDK8.1 D3D10 descriptor subset. Resource and allocation copies
 * must agree; the OS owns handles/PFNs and supplies the opened allocation.
 * No keyed mutex, GDI-compatible, NT-handle or cross-adapter claim. */
#define GEO_SHARED_MAGIC 0x31534847u
#define GEO_SHARED_MISC 2u
#define GEO_SHARED_SRV 8u
#define GEO_SHARED_RT 32u
#define GEO_SHARED_BGRA 87u
#define GEO_SHARED_RGBA 28u
struct geo_shared_surface {
 struct geo_allocation_data base;
 uint32_t magic,bind,format,dimension,usage,map_flags,misc,reserved;
};
static inline int geo_shared_surface_valid(const struct geo_shared_surface *s) {
 const struct geo_surface_desc *d;
 if(!s||s->base.size!=sizeof(*s)||s->base.version!=3||s->magic!=GEO_SHARED_MAGIC||
    s->dimension!=3||s->usage||s->map_flags||s->misc!=GEO_SHARED_MISC||s->reserved||
    !s->base.logical_width||!s->base.logical_height)return 0;
 d=&s->base.storage;
 if(!geo_desc_valid(d))return 0;
 if(s->bind==GEO_SHARED_RT)return s->format==GEO_SHARED_BGRA&&d->usage==GEO_USAGE_RENDER_TARGET&&
  d->format==GEO_FORMAT_A8R8G8B8&&d->width==800&&d->height==A04_NATIVE_FRAME_HEIGHT&&d->pitch==3200&&
  s->base.logical_width<=d->width&&s->base.logical_height<=d->height;
 if(s->bind==GEO_SHARED_SRV)return d->usage==GEO_USAGE_TEXTURE2D&&
  ((s->format==GEO_SHARED_BGRA&&d->format==GEO_FORMAT_A8R8G8B8)||
   (s->format==GEO_SHARED_RGBA&&d->format==GEO_FORMAT_A8B8G8R8))&&
  s->base.logical_width==d->width&&s->base.logical_height==d->height&&
  d->pitch==geo_texture_default_pitch(d->width);
 return 0;
}
static inline int geo_shared_surface_equal(const struct geo_shared_surface *a,const struct geo_shared_surface *b) {
 /* All fields are uint32_t and the wire has no padding or pointers. */
 return geo_shared_surface_valid(a)&&geo_shared_surface_valid(b)&&!memcmp(a,b,sizeof(*a));
}
#endif
