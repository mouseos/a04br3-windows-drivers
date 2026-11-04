/* SPDX-License-Identifier: MIT */
#ifndef A04_RENDER_KICK_POLICY_H
#define A04_RENDER_KICK_POLICY_H
#include "pvr_rogue_fwif.h"
/* Diagnostic only. Enabling this merely allows the paired KCCB command; it
 * does not validate its payload, establish VA ownership, or prove cleanup. */
#ifndef A04_GE8300_RENDER_EXPERIMENT
#define A04_GE8300_RENDER_EXPERIMENT 0
#endif
static int a04_gpu_kick_type_allowed(uint32_t type)
{
    if (type == ROGUE_FWIF_KCCB_CMD_KICK) return 1;
#if A04_GE8300_RENDER_EXPERIMENT
    if (type == ROGUE_FWIF_KCCB_CMD_COMBINED_GEOM_FRAG_KICK) return 1;
#endif
    return 0;
}
#if A04_GE8300_RENDER_EXPERIMENT
/* Never infer cleanup completion from the KCCB execution counter. */
static int a04_gpu_cleanup_return(uint32_t value)
{
    if (value & ROGUE_FWIF_KCCB_RTN_SLOT_POLL_FAILURE) return -1;
    if (!(value & ROGUE_FWIF_KCCB_RTN_SLOT_CMD_EXECUTED)) return 0;
    if (value & ROGUE_FWIF_KCCB_RTN_SLOT_CLEANUP_BUSY) return 1;
    return 2;
}
#endif
#endif
