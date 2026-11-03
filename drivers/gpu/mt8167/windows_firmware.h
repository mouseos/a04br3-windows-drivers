/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GE8300_WINDOWS_FIRMWARE_H
#define A04_GE8300_WINDOWS_FIRMWARE_H
#include "windows_hw.h"
struct a04_gpu_firmware_state {
    struct a04_gpu_dma_memory memory;
    struct a04_gpu_register_context registers;
    struct a04_pvr_register_io io;
    struct a04_pvr_fw_plan plan;
    struct a04_pvr_runtime runtime;
    struct a04_pvr_mips_addresses remap;
    BOOLEAN processor_released;
    BOOLEAN firmware_started;
    BOOLEAN queue_faulted;
};
/* PASSIVE_LEVEL only. The caller owns the PnP resources and DMA adapter.
 * Input state must be zeroed. The firmware image must remain accessible for
 * the duration of the call. A mismatched BVNC is denied unless requested.
 * After the MIPS processor is released, allocations are retained on every
 * result until the caller proves GPU DMA has stopped; device removal and
 * power-management integration must implement that lifecycle first. */
NTSTATUS a04_gpu_firmware_boot(struct a04_gpu_firmware_state *state,
        PDMA_ADAPTER adapter, volatile void *registers, ULONG register_length,
        const uint8_t *image, size_t image_size, uint64_t actual_bvnc,
        BOOLEAN allow_mismatch, ULONG core_hz);
/* Caller must have stopped new jobs and received a firmware power-off
 * acknowledgement. Retains all allocations if quiescence checks fail. */
NTSTATUS a04_gpu_firmware_stop(struct a04_gpu_firmware_state *state,
        BOOLEAN poweroff_acknowledged);
/* Caller serializes all firmware commands and prevents new GPU work during
 * shutdown. Any error leaves the DMA buffers allocated. */
NTSTATUS a04_gpu_firmware_health(struct a04_gpu_firmware_state *state);
NTSTATUS a04_gpu_firmware_shutdown(struct a04_gpu_firmware_state *state);
#endif
