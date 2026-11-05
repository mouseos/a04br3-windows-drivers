/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 22.40.54.30 vendor m1.11 firmware CPU staging.
 * Adapted from mouseos/xnu_kernel_benesse_a04br3, commit
 * 2374d4308c1e6bac25c9b490fe4063f919a41060, A04BR3RogueFirmware.h;
 * allocation profile and table mapping from A04BR3RogueMemory.h at
 * f5d43bf1677955e7e367545b9bbbd6449776cdda. No MMIO or GPU start.
 */
#ifndef A04_VENDOR_M1_11_STAGE_H
#define A04_VENDOR_M1_11_STAGE_H
#include "A04BR3RogueFirmware.h"
#include "A04BR3RogueMipsMMU.h"
enum { A04_VENDOR_TABLE_BYTES=0x8000, A04_VENDOR_CODE_BYTES=0x19000,
       A04_VENDOR_DATA_BYTES=0x5000, A04_VENDOR_DATA_HEAP_OFFSET=0x32000,
       A04_VENDOR_BOOT_DATA_OFFSET=0x3000, A04_VENDOR_STACK_OFFSET=0x4000 };
struct a04_vendor_buffer { void *cpu; uint64_t dma; uint32_t bytes; };
/* Stages the exact board profile into caller-owned common buffers. The
 * Windows caller must clean cache for all buffers and separately construct
 * m1.11 FWIF runtime objects before it may consider releasing GPU reset. */
int a04_vendor_m1_11_stage(const void *image,size_t image_bytes,
                           const struct a04_vendor_buffer buffers[3]);
#endif
