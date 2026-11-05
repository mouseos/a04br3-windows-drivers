/* SPDX-License-Identifier: MIT
 * Conditions derived from pinned Mesa shared-state loops: store step 1024
 * dwords, load step 256, state payload 4096, size in 16-dword blocks. */
#include "render_shared_size.h"
int a04_render_check_bounded_shared_size(const uint8_t *state,size_t size,
    struct a04_render_shared_size_result *out)
{
    struct a04_render_shared_size_result result;uint32_t blocks;
    if (!state || size<16432u || !out) return -1;
    blocks=(uint32_t)state[0]|((uint32_t)state[1]<<8)|((uint32_t)state[2]<<16)|((uint32_t)state[3]<<24);
    if (blocks>256u) return -1;
    result.blocks=blocks;result.dwords=blocks*16u;result.bytes=result.dwords*4u;
    *out=result;return 0;
}
int a04_render_check_saved_shared_size(const uint8_t *state,size_t size,
    struct a04_render_shared_size_result *out)
{
    struct a04_render_shared_size_result result;uint32_t blocks;
    if (!state || size<16432u || !out) return -1;
    blocks=(uint32_t)state[0]|((uint32_t)state[1]<<8)|((uint32_t)state[2]<<16)|((uint32_t)state[3]<<24);
    /* Both original loops terminate within the allocation only if the store
     * value is a multiple of 64 blocks and no greater than 256 blocks. */
    if (blocks>256u || (blocks&63u)) return -1;
    result.blocks=blocks;result.dwords=blocks*16u;result.bytes=result.dwords*4u;
    *out=result;return 0;
}
