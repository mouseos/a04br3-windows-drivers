/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_PRESENT_COPY_FENCE_H
#define A04_PRESENT_COPY_FENCE_H
#include <stddef.h>
#include "../ge8300_status.h"
/* Local task and GP-fence evidence is necessary for this CPU-staged copy;
 * it is not a WDDM scheduling fence or a display DMA visibility guarantee. */
static int a04_present_copy_fence_valid(const struct a04_ge8300_fenced_copy *r,
                                        size_t bytes)
{
    return r && bytes==sizeof(*r) && r->version==1 && r->task_sequence && r->flush_sequence;
}
#endif
