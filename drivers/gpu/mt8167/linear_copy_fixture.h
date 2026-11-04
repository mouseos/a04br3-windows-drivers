/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_LINEAR_COPY_FIXTURE_H
#define A04_GE8300_LINEAR_COPY_FIXTURE_H
#include "copy_fixture.h"
#define A04_GPU_LINEAR_COPY_PAGES 294u
#define A04_GPU_LINEAR_SOURCE_PAGE 281u
#define A04_GPU_LINEAR_DEST_PAGE 287u
extern const struct a04_gpu_copy_region a04_gpu_linear_copy_regions[];
extern const size_t a04_gpu_linear_copy_region_count;
int a04_gpu_linear_copy_fixture(void *pages,size_t size,struct rogue_fwif_cmd_transfer *command);
#endif
