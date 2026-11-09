/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_SCANOUT_OBSERVE356_H
#define GEO_SCANOUT_OBSERVE356_H
/* Read-only observation of the actual selected VidMm backing. It cannot
 * submit, select, acknowledge, or manufacture a GPU/display completion. */
#define GEO_SCANOUT_OBSERVE356_MAGIC 0x36355347u
#define GEO_SCANOUT_OBSERVE356_BYTES 4096000u
struct geo_scanout_observe356 {
 unsigned int size,version,magic,flags,width,height,pitch,bytes;
 unsigned int frame_low,frame_high,completed_fence,submitted_fence;
 unsigned int pixel_hash,pfn_hash,reserved[2];
 unsigned char pixels[GEO_SCANOUT_OBSERVE356_BYTES];
};
#endif
