/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_COPY_QUEUE_H
#define A04_NATIVE_COPY_QUEUE_H
#include "native_copy_job.h"
#include "native_ccb.h"
struct a04_native_copy_queue_input {
 uint32_t context_fw,sync_fw,previous_value;
 uint32_t read,write,dependency,external_job,internal_job;
};
struct a04_native_copy_queue_result {
 uint32_t write_offset,task_offset,padding_bytes,completion_value;
 RGXFWIF_KCCB_CMD kick;
};
/* Build a complete TQ/UPDATE group in an ordinary 4KiB CPU ring shadow.
 * Conservative drained-ring policy matches proven DOD copy publication.
 * Packet and output stay unchanged on failure. Workspace must be another
 * disjoint 4KiB array; it alone may change before final commit. No MMIO or
 * completion notification. Real KCCB/CCCB/UFO observations belong to owner. */
int a04_native_copy_queue_build(void *ring,void *workspace,
 const struct a04_native_copy_queue_input *,const struct a04_native_copy_job_image *,
 struct a04_native_copy_queue_result *);
#endif
