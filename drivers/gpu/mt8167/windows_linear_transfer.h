/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GE8300_WINDOWS_LINEAR_TRANSFER_H
#define A04_GE8300_WINDOWS_LINEAR_TRANSFER_H
#include "windows_transfer_mapping.h"

/* Synchronous fixed 16 KiB transfer through the established linear-copy
 * fixture. Both CPU addresses name six whole pages of nonpaged Windows common
 * buffer memory corresponding exactly to the supplied adapter DMA pages;
 * payload begins one page in, with guard pages surrounding it. The validated
 * transfer command must reference the two fixed VAs and owned immutable shader
 * resources. Caller owns and serializes firmware, queue, page tables and all
 * DMA, supplies every protected GPU DMA range, and stays at PASSIVE_LEVEL.
 * No OS scatter/gather acquisition or framebuffer mapping is performed here.
 * On any poisoned mapping/queue retain BOTH buffers and all GPU resources
 * until GPU DMA quiescence is independently confirmed. Success guarantees
 * fence completion, CPU destination synchronization, scratch restoration and
 * MMU invalidation, not visual output or correctness of pixel interpretation. */
NTSTATUS a04_windows_linear_transfer(struct a04_windows_transfer_mapping *mapping,
    const struct rogue_fwif_cmd_transfer *command,uint32_t job_id,
    const uint64_t *source_pages,const uint64_t *dest_pages,
    void *source_cpu,void *dest_cpu,
    const struct a04_transfer_range *protected_ranges,size_t protected_count);
#endif
