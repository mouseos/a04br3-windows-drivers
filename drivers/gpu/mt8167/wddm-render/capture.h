/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_CAPTURE_H
#define GEO_CAPTURE_H
#include <ntddk.h>
#include "clear_protocol.h"
#include "copy_region.h"
#include "frontend/draw_link_abi.h"
#include "frontend/texture_draw_abi.h"
#include "frontend/shader_draw_abi.h"
/* Fixed semantic packet only; source is the runtime's untouched USER address.
 * Output is a caller-owned kernel packet. No direct user dereference. */
NTSTATUS GeoCaptureCopyRegionPacket(struct geo_copy_region_packet *,const void *);
NTSTATUS GeoCaptureClearPacket(struct geo_clear_packet *output,const void *user_source);
NTSTATUS GeoCaptureDrawPacket(struct geo_draw_packet **output,const void *user_source,ULONG bytes);
void GeoReleaseDrawCapture(struct geo_draw_packet *);
#endif
