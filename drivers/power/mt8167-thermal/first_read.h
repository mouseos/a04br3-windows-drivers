/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef MT8167_THERMAL_FIRST_READ_H
#define MT8167_THERMAL_FIRST_READ_H
#include <stdint.h>
struct a04_thermal_first_read {unsigned attempted;int32_t status;};
/* Caller serializes requests with the D0 power-managed sequential WDF queue.
 * Attempt BEFORE callback; a failed/uncertain parent operation is never retried.
 * This is a status cache, not a temperature/validity/freshness substitute. */
static int32_t a04_thermal_collect_once(struct a04_thermal_first_read *s,void *context,int32_t (*collect)(void *))
{
 if(!s->attempted){s->attempted=1;s->status=collect(context);}
 return s->status;
}
#endif
