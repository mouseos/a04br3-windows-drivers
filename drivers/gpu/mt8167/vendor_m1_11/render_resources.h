/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 m1.11 firmware context layout, adapted from mouseos/
 * xnu_kernel_benesse_a04br3 A04BR3RogueRenderResources.h,
 * f5d43bf1677955e7e367545b9bbbd6449776cdda.
 * Cold shadow-memory construction only. It does not submit work or own DMA.
 */
#ifndef A04_VENDOR_RENDER_RESOURCES_H
#define A04_VENDOR_RENDER_RESOURCES_H
#include <stddef.h>
#include <stdint.h>
#define A04_VENDOR_RENDER_BANK_BYTES 65536u
#define A04_VENDOR_RENDER_FW_OFFSET 0x80000u
#define A04_VENDOR_CLIENT_RING_BYTES 4096u
#define A04_VENDOR_RENDER_SCRATCH_VA UINT64_C(0x65c0000000)
struct a04_vendor_render_layout {
    uint32_t context, memory, control[4], commands[4], sync, gp, tq, used;
};
/* bank is ordinary CPU-writable shadow memory, never Device memory. pc_dma
 * names the separately constructed Rogue BIF page catalogue. The caller owns
 * the memory and copies it to a distinct DMA bank using aligned device writes.
 * The scratch GPU VA must already be backed in that catalogue before use. */
int a04_vendor_render_build(void *bank, size_t bank_bytes, uint64_t bank_dma,
    uint64_t pc_dma, uint64_t scratch_va, uint32_t first_context_id,
    struct a04_vendor_render_layout *layout);
/* Cold MIPS mapping only: do not modify tables after firmware starts. */
int a04_vendor_render_map_cold(uint32_t *table, size_t table_bytes,
    uint64_t table_dma, const void *bank, uint64_t bank_dma);
#endif
