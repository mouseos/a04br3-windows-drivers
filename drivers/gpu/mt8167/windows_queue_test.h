/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_WINDOWS_QUEUE_TEST_H
#define A04_GE8300_WINDOWS_QUEUE_TEST_H
#include "windows_firmware.h"
/* The allocation is retained on every error until firmware is stopped and
 * the GPU's DMA has been quiesced. The owner must keep the adapter alive. */
struct a04_gpu_queue_test_memory { PDMA_ADAPTER adapter; void *cpu; PHYSICAL_ADDRESS logical; };
NTSTATUS a04_gpu_queue_test(struct a04_gpu_firmware_state *state,
        struct a04_gpu_queue_test_memory *memory);
NTSTATUS a04_gpu_queue_test_free(struct a04_gpu_firmware_state *state,
        struct a04_gpu_queue_test_memory *memory);
#endif
