/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SCANOUT_LIFETIME_H
#define A04_SCANOUT_LIFETIME_H
#include <stdint.h>

/* Policy only: the owner serializes calls and holds both device mappings.
 * Display DMA addresses must come from the display adapter, not GPU DMA or
 * CPU physical addresses. No function here performs MMIO or proves an event. */
enum a04_scanout_phase { A04_SCANOUT_EMPTY, A04_SCANOUT_READY,
    A04_SCANOUT_RENDERING, A04_SCANOUT_RENDERED, A04_SCANOUT_SWITCHING,
    A04_SCANOUT_ACTIVE, A04_SCANOUT_STOPPING, A04_SCANOUT_RETAIN };
struct a04_scanout_lifetime {
    enum a04_scanout_phase phase;
    uint64_t gpu_dma, display_dma, bytes, fence;
};
static int a04_scanout_bind(struct a04_scanout_lifetime *s, uint64_t gpu_dma,
    uint64_t display_dma, uint64_t bytes, int gpu_mapping_owned,
    int display_mapping_owned)
{
    if (!s || s->phase != A04_SCANOUT_EMPTY || !gpu_mapping_owned ||
        !display_mapping_owned || !gpu_dma || !display_dma || !bytes ||
        gpu_dma > UINT64_MAX - (bytes - 1) ||
        display_dma > UINT32_MAX || bytes - 1 > UINT32_MAX - display_dma)
        return 0;
    s->gpu_dma=gpu_dma; s->display_dma=display_dma; s->bytes=bytes;
    s->fence=0; s->phase=A04_SCANOUT_READY; return 1;
}
static int a04_scanout_submit(struct a04_scanout_lifetime *s, uint64_t fence)
{
    if (!s || s->phase != A04_SCANOUT_READY || !fence) return 0;
    s->fence=fence; s->phase=A04_SCANOUT_RENDERING; return 1;
}
/* Exact submission completion plus device-to-device visibility are separate
 * obligations. A timeout or uncertain completion quarantines both mappings. */
static int a04_scanout_complete(struct a04_scanout_lifetime *s, uint64_t fence,
    int completed, int visible)
{
    if (!s || s->phase != A04_SCANOUT_RENDERING || fence != s->fence) return 0;
    if (!completed || !visible) { s->phase=A04_SCANOUT_RETAIN; return 0; }
    s->phase=A04_SCANOUT_RENDERED; return 1;
}
/* Once a register transaction is attempted, an error cannot establish which
 * buffer OVL uses. Retain it until independent display quiescence. */
static int a04_scanout_switch_begin(struct a04_scanout_lifetime *s)
{
    if (!s || s->phase != A04_SCANOUT_RENDERED) return 0;
    s->phase=A04_SCANOUT_SWITCHING; return 1;
}
static int a04_scanout_switch_end(struct a04_scanout_lifetime *s, int latched)
{
    if (!s || s->phase != A04_SCANOUT_SWITCHING) return 0;
    s->phase=latched ? A04_SCANOUT_ACTIVE : A04_SCANOUT_RETAIN;
    return latched != 0;
}
static int a04_scanout_stop_begin(struct a04_scanout_lifetime *s)
{
    if (!s || s->phase == A04_SCANOUT_EMPTY || s->phase == A04_SCANOUT_STOPPING) return 0;
    s->phase=A04_SCANOUT_STOPPING; return 1;
}
/* Neither a GPU fence nor an OVL disable write proves both engines idle. */
static int a04_scanout_stop_end(struct a04_scanout_lifetime *s,
    int gpu_quiesced, int display_quiesced)
{
    if (!s || s->phase != A04_SCANOUT_STOPPING) return 0;
    if (!gpu_quiesced || !display_quiesced) { s->phase=A04_SCANOUT_RETAIN; return 0; }
    s->phase=A04_SCANOUT_EMPTY; s->gpu_dma=0; s->display_dma=0;
    s->bytes=0; s->fence=0; return 1;
}
#endif
