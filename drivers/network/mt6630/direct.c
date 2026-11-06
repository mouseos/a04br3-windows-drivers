/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "direct.h"
#include "sdio_args.h"
int a04_sdio_direct52(struct a04_sdio_direct *s, unsigned write, unsigned fn,
                      unsigned raw, unsigned address, uint8_t *data)
{
 uint32_t arg, r5 = 0;
 if (!s || !s->owner || !s->request || !data || s->poisoned ||
     s->owner->poisoned || !s->owner->active ||
     s->generation != s->owner->generation ||
     a04_sdio_cmd52(write, fn, raw, address, write ? *data : 0, &arg))
  return -1;
 if (s->request(s->context, arg, &r5) || !a04_sdio_r5_ok(r5) ||
     s->owner->poisoned || !s->owner->active ||
     s->generation != s->owner->generation) {
  s->poisoned = 1;
  a04_wifi_fault(s->owner);
  return -2;
 }
 if (!write || raw) *data = (uint8_t)r5;
 return 0;
}
