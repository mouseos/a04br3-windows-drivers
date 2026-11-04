/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_COPY_FIXTURE_H
#define A04_GE8300_COPY_FIXTURE_H
#include <stdint.h>
#include <stddef.h>
#include "pvr_rogue_fwif_client.h"
#define A04_GPU_COPY_PAGES 290u
#define A04_GPU_COPY_GROUPS 4u
#define A04_GPU_COPY_SOURCE_PAGE 281u
#define A04_GPU_COPY_DEST_PAGE 285u
struct a04_gpu_copy_region { uint64_t va; uint16_t page_offset, page_count; uint8_t group; };
extern const struct a04_gpu_copy_region a04_gpu_copy_regions[];
extern const size_t a04_gpu_copy_region_count;
int a04_gpu_copy_fixture(void *pages,size_t size,struct rogue_fwif_cmd_transfer *command);
#endif
