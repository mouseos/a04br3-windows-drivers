/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include "render_kick_policy.h"
int main(void)
{
    assert(a04_gpu_kick_type_allowed(ROGUE_FWIF_KCCB_CMD_KICK));
#if A04_GE8300_RENDER_EXPERIMENT
    assert(a04_gpu_kick_type_allowed(ROGUE_FWIF_KCCB_CMD_COMBINED_GEOM_FRAG_KICK));
    assert(a04_gpu_cleanup_return(0)==0);
    assert(a04_gpu_cleanup_return(ROGUE_FWIF_KCCB_RTN_SLOT_CMD_EXECUTED)==2);
    assert(a04_gpu_cleanup_return(ROGUE_FWIF_KCCB_RTN_SLOT_CMD_EXECUTED | ROGUE_FWIF_KCCB_RTN_SLOT_CLEANUP_BUSY)==1);
    assert(a04_gpu_cleanup_return(ROGUE_FWIF_KCCB_RTN_SLOT_POLL_FAILURE)==-1);
#else
    assert(!a04_gpu_kick_type_allowed(ROGUE_FWIF_KCCB_CMD_COMBINED_GEOM_FRAG_KICK));
#endif
    assert(!a04_gpu_kick_type_allowed(0));
    assert(!a04_gpu_kick_type_allowed(ROGUE_FWIF_KCCB_CMD_CLEANUP));
    return 0;
}
