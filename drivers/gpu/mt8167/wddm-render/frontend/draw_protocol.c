/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "draw_validation_private.h"
int32_t geo_draw_validate_capture(const struct geo_draw_request *r,const void *blob,uint32_t bytes)
{ return capture(r,blob,bytes,sizeof(*r),GEO_DRAW_ABI_VERSION,16); }
