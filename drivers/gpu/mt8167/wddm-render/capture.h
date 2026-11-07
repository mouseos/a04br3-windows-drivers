/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_CAPTURE_H
#define GEO_CAPTURE_H
#include <ntddk.h>
#include "clear_protocol.h"
#include "frontend/draw_abi.h"
/* Fixed semantic packet only; source is the runtime's untouched USER address.
 * Output is a caller-owned kernel packet. No direct user dereference. */
NTSTATUS GeoCaptureClearPacket(struct geo_clear_packet *output,const void *user_source);
NTSTATUS GeoCaptureDrawPacket(struct geo_draw_packet **output,const void *user_source,UINT bytes);
void GeoReleaseDrawCapture(struct geo_draw_packet *);
#endif
