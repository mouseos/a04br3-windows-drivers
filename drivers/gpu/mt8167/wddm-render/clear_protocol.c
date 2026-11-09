/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "clear_protocol.h"
int geo_desc_valid(const struct geo_surface_desc *d) {
 uint64_t bytes;
 if(d&&d->size==sizeof(*d)&&d->version==GEO_TEXTURE_STORAGE_VERSION&&!d->reserved&&d->usage==GEO_USAGE_TEXTURE2D)
  return d->format==GEO_FORMAT_A8R8G8B8&&d->width&&d->width<=128&&d->height&&d->height<=128&&
   !(d->pitch&3u)&&d->pitch>=d->width*4u&&(uint64_t)d->pitch*d->height<=65536u;
 if(d&&d->size==sizeof(*d)&&d->version==GEO_ABI_VERSION&&!d->reserved&&d->usage==GEO_USAGE_VERTEX_BUFFER)
  return d->format==GEO_FORMAT_VERTEXDATA&&d->height==1u&&d->width==d->pitch&&
   d->width>=4u&&d->width<=GEO_VERTEX_MAX_BYTES;
 if(d&&d->size==sizeof(*d)&&d->version==GEO_ABI_VERSION&&!d->reserved&&d->usage==GEO_USAGE_INDEX_BUFFER)
  return d->height==1u&&d->width==d->pitch&&d->width<=GEO_VERTEX_MAX_BYTES&&
   ((d->format==GEO_FORMAT_INDEX16&&d->width>=2u&&!(d->width&1u))||
    (d->format==GEO_FORMAT_INDEX32&&d->width>=4u&&!(d->width&3u)));
 if(!d||d->size!=sizeof(*d)||d->version!=GEO_ABI_VERSION||!d->width||!d->height||
    d->width>1024||d->height>(A04_GE8300_UNIFIED_DISPLAY356?1280u:1024u)||d->pitch<d->width*4u||(d->pitch&3u)||d->reserved||
    (d->usage!=GEO_USAGE_RENDER_TARGET&&d->usage!=GEO_USAGE_STAGING))return 0;
 /* Standard STAGINGSURFACE is implicitly X8R8G8B8. geo_create keeps every
  * staging allocation CPU-only; no X8 rendering or GPU owner format is added. */
 if(d->format!=GEO_FORMAT_A8R8G8B8&&
    !(d->usage==GEO_USAGE_STAGING&&d->format==GEO_FORMAT_X8R8G8B8))return 0;
 bytes=(uint64_t)d->pitch*d->height;return bytes<=4u*1024u*1024u;
}
int geo_packet_valid(const struct geo_clear_packet *p,uint32_t count) {
 return p&&p->magic==GEO_CLEAR_MAGIC&&p->size==sizeof(*p)&&p->version==GEO_ABI_VERSION&&
   p->allocation_index<count&&!p->flags&&!p->reserved[0]&&!p->reserved[1];
}
