/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 GE8300 Rogue MIPS firmware image layout support.
 * Firmware format and layout are described by the upstream PowerVR driver;
 * see the source references in README.md. No hardware is accessed here. */
#ifndef A04_GE8300_FIRMWARE_H
#define A04_GE8300_FIRMWARE_H
#include <stdint.h>
#include <stddef.h>
#define A04_PVR_HEAP_SIZE (16u * 1024u * 1024u)
#define A04_PVR_MAX_LAYOUT 8u
#define A04_PVR_BVNC UINT64_C(0x001600280036001e)
struct a04_pvr_section {
    uint32_t id, type, base, max_size, size, offset;
};
struct a04_pvr_fw_plan {
    uint64_t firmware_bvnc;
    uint32_t code_size, data_offset, data_size, section_count;
    uint32_t program_offset, program_size, program_count;
    struct a04_pvr_section section[A04_PVR_MAX_LAYOUT];
};
enum a04_pvr_fw_result {
    A04_PVR_FW_OK = 0, A04_PVR_FW_INVALID = 1,
    A04_PVR_FW_INCOMPATIBLE = 2, A04_PVR_FW_LAYOUT = 3,
    A04_PVR_FW_ELF = 4, A04_PVR_FW_OUTPUT = 5
};
/* The BVNC mismatch is rejected unless explicitly allowed by the caller. */
enum a04_pvr_fw_result a04_pvr_fw_plan(const uint8_t *image, size_t length,
        uint64_t actual_bvnc, int allow_mismatch, struct a04_pvr_fw_plan *plan);
/* No writes occur on validation failure; output must be a distinct 16 MiB heap. */
enum a04_pvr_fw_result a04_pvr_fw_load(const uint8_t *image, size_t length,
        uint64_t actual_bvnc, int allow_mismatch, uint8_t *heap, size_t heap_size,
        struct a04_pvr_fw_plan *plan);
struct a04_pvr_mips_addresses {
    uint32_t boot_code, boot_data, exception_code;
};
/* Prepare the 4-page MIPS page table and boot data for two distinct,
 * physically contiguous and page-aligned allocations below 4 GiB.
 * The caller owns DMA allocation, cache synchronization and GPU startup. */
enum a04_pvr_fw_result a04_pvr_mips_prepare(const struct a04_pvr_fw_plan *plan,
        uint8_t *heap, size_t heap_size, uint64_t heap_physical,
        uint32_t *page_table, size_t page_table_bytes, uint64_t page_table_physical,
        struct a04_pvr_mips_addresses *addresses);
struct a04_pvr_runtime {
    uint32_t kernel_ccb_control, kernel_ccb, kernel_return_slots;
    uint32_t firmware_ccb_control, firmware_ccb, os_data, system_data;
    uint32_t mmu_sync, trace_control, trace[2];
    uint32_t os_init, system_init, next_free;
};
/* Construct firmware-shared queues and initialization objects in the heap.
 * The caller must synchronize the heap for DMA before starting firmware. */
enum a04_pvr_fw_result a04_pvr_runtime_prepare(const struct a04_pvr_fw_plan *plan,
        uint8_t *heap, size_t heap_size, uint64_t heap_physical, uint32_t core_hz,
        struct a04_pvr_runtime *runtime);
/* MMIO callbacks must provide the hardware's required 64-bit register access
 * semantics, ordering and readback barriers; delay is in microseconds. */
struct a04_pvr_register_io {
    void *context;
    uint32_t (*read32)(void *, uint32_t);
    uint64_t (*read64)(void *, uint32_t);
    void (*write32)(void *, uint32_t, uint32_t);
    void (*write64)(void *, uint32_t, uint64_t);
    void (*delay_us)(void *, unsigned);
};
/* Release the MIPS processor only after the caller has synchronized all
 * firmware heap and page-table writes for the GPU. */
enum a04_pvr_fw_result a04_pvr_mips_start(const struct a04_pvr_register_io *io,
        uint64_t core_bvnc, const struct a04_pvr_mips_addresses *addresses);
#endif
