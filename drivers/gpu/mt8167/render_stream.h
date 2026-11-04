/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_RENDER_STREAM_H
#define A04_GE8300_RENDER_STREAM_H
#include <stddef.h>
#include <stdint.h>
#include "pvr_rogue_fwif_client.h"
#define A04_RENDER_DRAW_INDIRECT (1u << 0)
#define A04_RENDER_OBJECT_LLS (1u << 1)
#define A04_RENDER_TESSELLATION (1u << 2)
#define A04_RENDER_FB_CDC_ZLS (1u << 3)
#define A04_RENDER_S7_TOP (1u << 4)
#define A04_RENDER_CLUSTER_GROUPING (1u << 5)
#define A04_RENDER_MULTICORE (1u << 6)
#define A04_RENDER_ZLS_SUBTILE (1u << 7)
#define A04_RENDER_ZLS_D24S8 (1u << 8)
#define A04_RENDER_QUIRK_49927 (1u << 9)
#define A04_RENDER_QUIRK_47217 (1u << 10)
/* Parse upstream command-stream layouts only; this does not validate GPU
 * addresses or make commands safe for submission. Feature and quirk bits must
 * come from the selected GPU/firmware, not user input. Supports only canonical
 * streams with no extension or one BRN49927 extension; devices requiring
 * other extensions are rejected. BRN47217 is explicitly rejected. Output is
 * atomic on malformed input. */
int a04_gpu_decode_geometry(const void *stream, size_t size, uint32_t capabilities,
                            struct rogue_fwif_cmd_geom *out);
int a04_gpu_decode_fragment(const void *stream, size_t size, uint32_t capabilities,
                            struct rogue_fwif_cmd_frag *out);
int a04_gpu_decode_render_static(const void *stream, size_t size,
                                 struct rogue_fwif_geom_registers_caswitch *out);
#endif
