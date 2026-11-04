/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GE8300_WINDOWS_TRANSFER_MAPPING_H
#define A04_GE8300_WINDOWS_TRANSFER_MAPPING_H
#include "windows_firmware.h"
#include "firmware_queue.h"
#include "transfer_mapping.h"
struct a04_windows_transfer_mapping {
    struct a04_transfer_mapping mapping;
    struct a04_gpu_firmware_state *firmware;
    struct a04_gpu_fw_queue *queue;
    uint32_t completed_fence;
    NTSTATUS last_status;
};
/* The leaf, queue and scratch pages must already be installed and owned by
 * the same live firmware context. Call at PASSIVE_LEVEL with the context lock
 * held; the caller updates completed_fence after each submitted transfer. */
NTSTATUS a04_windows_transfer_mapping_init(struct a04_windows_transfer_mapping *context,
    struct a04_gpu_firmware_state *firmware,struct a04_gpu_fw_queue *queue,
    uint64_t *leaf,const uint64_t *scratch_source,const uint64_t *scratch_dest,
    uint32_t completed_fence);
#endif
