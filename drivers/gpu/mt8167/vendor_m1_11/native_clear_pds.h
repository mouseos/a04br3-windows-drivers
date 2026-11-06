/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_CLEAR_PDS_H
#define A04_M111_NATIVE_CLEAR_PDS_H
#include <stddef.h>
#include <stdint.h>
#define A04_M111_BACKGROUND_PDS_BYTES 36u
/* Single-sample/no-extra-task-flags subset of vendor GLES 0x2025c4 and
 * 0x2047e4. USC belongs to the existing BRN65273 USC heap at 0xba00000000.
 * This does not supply, validate, or allocate the USC program. */
int a04_m111_patch_background_pds(void *cpu,size_t bytes,
    uint64_t usc_gpu,uint32_t temporary_count);
/* GLES SetupPixelEventProgram 0x1c3cf4 -> 0x202600 with DMA count zero:
 * the EOT data segment is exactly the USC task (two words). The separate
 * pixel-event PDS code/register binding is not constructed here. */
int a04_m111_build_eot_task_data(void *cpu,size_t bytes,
    uint64_t usc_gpu,uint32_t temporary_count);
#endif
