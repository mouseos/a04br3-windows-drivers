/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_TRANSFER_STREAM_H
#define A04_GE8300_TRANSFER_STREAM_H
#include <stddef.h>
#include <stdint.h>
#include "pvr_rogue_fwif_client.h"
#define A04_TRANSFER_S7_TOP UINT32_C(1)
#define A04_TRANSFER_MULTICORE UINT32_C(2)
/* Decode the little-endian upstream PowerVR transfer stream into a firmware command.
 * Features must come from verified hardware/firmware capabilities. Structural
 * validation is not GPU command, memory-access, or security validation. */
int a04_gpu_decode_transfer(const void *stream,size_t stream_size,uint32_t features,
    struct rogue_fwif_cmd_transfer *out);
#endif
