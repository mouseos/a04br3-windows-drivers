/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_FIRMWARE_OBJECTS_H
#define A04_GE8300_FIRMWARE_OBJECTS_H
#include <stddef.h>
#include <stdint.h>
struct a04_gpu_fw_arena { uint8_t *heap; size_t size,next,limit; uint64_t dma; };
struct a04_gpu_fw_object { void *cpu; size_t size; uint32_t fw_addr; uint64_t dma; };
/* The caller serializes arena access. Objects remain allocated until all GPU
 * work and firmware references are quiesced; this arena does not free or
 * reuse individual objects. DMA addresses come from the adapter mapping. */
int a04_gpu_fw_arena_init(struct a04_gpu_fw_arena *arena,void *heap,size_t heap_size,
    uint64_t heap_dma,size_t first_free);
int a04_gpu_fw_alloc(struct a04_gpu_fw_arena *arena,size_t size,size_t alignment,
    struct a04_gpu_fw_object *object);
/* Caller must create and synchronize the root table and the BRN65273 zero
 * pages before a firmware context can use the resulting object. */
int a04_gpu_fw_memory_context(struct a04_gpu_fw_arena *arena,uint64_t root_dma,
    struct a04_gpu_fw_object *object);
#endif
