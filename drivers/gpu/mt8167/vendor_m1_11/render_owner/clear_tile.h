/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Offline native format 0x22 zero-clear tile, derived from XNU
 * A04BR3RogueClearTile.h and REA evidence ev_a8b70e873cd2536b115a348ff0bc99a5802b311ce09299c99f127d837cf5ef13.
 * This does not grant mapping ownership or submit a GPU command. */
#ifndef A04_VENDOR_CLEAR_TILE_H
#define A04_VENDOR_CLEAR_TILE_H
#include "clear_image.h"
struct a04_vendor_clear_tile_request { uint32_t width,height,pitch_pixels,x,y; };
struct a04_vendor_clear_tile { struct a04_vendor_clear_image image; uint32_t first,end,row_bytes; };
/* Exactly one 32x32 tile in the owned 64 KiB scratch map. Pitch and origins
 * must be 32-pixel aligned. On failure the output remains untouched. */
int a04_vendor_clear_tile_build(struct a04_vendor_clear_tile *out,
                                const struct a04_vendor_clear_tile_request *request);
#endif
