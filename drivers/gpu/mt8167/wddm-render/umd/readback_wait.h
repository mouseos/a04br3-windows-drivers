/* SPDX-License-Identifier: MIT. No GPU completion is synthesized. */
#ifndef GEO_READBACK_WAIT_H
#define GEO_READBACK_WAIT_H
#include <stdint.h>
#define GEO_READBACK_BUDGET_MS 30000u
#define GEO_READBACK_DEVICE_HUNG ((int32_t)0x887a0006u)
struct geo_readback_wait_ops {void *context;int32_t (*attempt)(void *);uint32_t (*clock_ms)(void *);void (*pause_ms)(void *);};
static int32_t geo_readback_wait(const struct geo_readback_wait_ops *o,int32_t busy0,int32_t busy1,int nonblocking) {
 uint32_t start=o->clock_ms(o->context);int32_t hr;
 for(;;){
  hr=o->attempt(o->context);if(hr>=0||((hr!=busy0)&&(hr!=busy1))||nonblocking)return hr;
  if((uint32_t)(o->clock_ms(o->context)-start)>=GEO_READBACK_BUDGET_MS)return GEO_READBACK_DEVICE_HUNG;
  o->pause_ms(o->context);
 }
}
#endif
