/* SPDX-License-Identifier: MIT */
#ifndef A04_RENDER_FIXED_COMMAND_H
#define A04_RENDER_FIXED_COMMAND_H
#include "render_execution.h"
#include "pvr_rogue_fwif_client.h"
/* Fixed 64x64, single-sample, clear-and-triangle probe; all inputs GPU VAs. */
struct a04_render_fixed_command_input {
    uint64_t vdm,border,scissor,target;
    uint64_t background_shader,background_code,background_data,partial_background_data,event_data;
};
/* Only constructs register words and validates address ownership. The caller
 * must establish contents, full dependency closure, synchronization and life. */
int a04_render_make_fixed_commands(const struct a04_render_fixed_command_input *input,
    const struct a04_render_owned_range *ranges,size_t count,
    struct rogue_fwif_cmd_geom *geom,struct rogue_fwif_cmd_frag *frag);
#endif
