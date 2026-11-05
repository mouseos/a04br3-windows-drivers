/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_THERMAL_OWNER_H
#define A04_SHARED_THERMAL_OWNER_H
#include <stdint.h>
/* Caller serializes with the parent's mutex. These flags are evidence from
 * real handoff and all ADC consumers, never from a build flag or ACPI alone. */
struct a04_shared_thermal_owner { uint32_t generation; int online,firmware_released,adc_exclusive,leased,poisoned; };
static inline int a04_shared_thermal_acquire(struct a04_shared_thermal_owner *s,uint32_t *generation)
{
 if(!s || !generation || !s->online || !s->firmware_released || !s->adc_exclusive ||
    s->leased || s->poisoned || s->generation==UINT32_MAX)return -1;
 s->generation++;s->leased=1;*generation=s->generation;return 0;
}
static inline int a04_shared_thermal_valid(const struct a04_shared_thermal_owner *s,uint32_t generation)
{ return s && s->online && s->leased && !s->poisoned && generation && s->generation==generation; }
static inline int a04_shared_thermal_release(struct a04_shared_thermal_owner *s,uint32_t generation)
{ if(!a04_shared_thermal_valid(s,generation))return -1;s->leased=0;return 0; }
static inline void a04_shared_thermal_stop(struct a04_shared_thermal_owner *s)
{ if(!s)return;if(s->leased)s->poisoned=1;s->online=0;s->firmware_released=0;s->adc_exclusive=0; }
#endif
