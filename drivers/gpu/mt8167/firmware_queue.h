/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_FIRMWARE_QUEUE_H
#define A04_GE8300_FIRMWARE_QUEUE_H
#include "firmware_objects.h"
#include "pvr_rogue_fwif.h"
#include "pvr_rogue_fwif_client.h"
struct a04_gpu_fw_queue {
    struct a04_gpu_fw_object control, ring, state, fence, context;
    uint32_t write_offset, wrap_mask;
};
/* Prepare one compute-context NULL command and a fence update. No command is
 * submitted here. Caller must synchronize every object, install the associated
 * VM and BRN65273 workaround, and retain objects until firmware cleanup and
 * GPU quiescence have been confirmed. fence must be observed separately: KCCB
 * execution alone does not mean the client command completed. */
int a04_gpu_fw_prepare_null_queue(struct a04_gpu_fw_arena *arena,const struct a04_gpu_fw_object *mem_context,
    uint32_t context_id,uint32_t job_id,struct a04_gpu_fw_queue *queue,
    struct rogue_fwif_kccb_cmd *kick);
/* The caller must validate all GPU addresses and command contents and obtain
 * isp_store_count from verified hardware features before this is used. */
int a04_gpu_fw_prepare_transfer_queue(struct a04_gpu_fw_arena *arena,const struct a04_gpu_fw_object *mem_context,
    uint32_t context_id,uint32_t job_id,unsigned isp_store_count,
    const struct rogue_fwif_cmd_transfer *transfer,struct a04_gpu_fw_queue *queue,
    struct rogue_fwif_kccb_cmd *kick);
/* Reuse an existing transfer queue after the prior fence has completed and
 * read_offset has caught up with write_offset. Caller first invalidates the
 * cached control/fence, serializes submissions, validates every command and
 * referenced GPU VA, and retains all resources. On success the caller cleans
 * the entire ring, then submits kick and waits for the new fence value. This
 * helper does not synchronize caches, submit work or establish DMA quiescence. */
int a04_gpu_fw_append_transfer(struct a04_gpu_fw_queue *queue,
    const struct rogue_fwif_cmd_transfer *transfer,uint32_t job_id,
    uint32_t completed_fence,struct rogue_fwif_kccb_cmd *kick);
#endif
