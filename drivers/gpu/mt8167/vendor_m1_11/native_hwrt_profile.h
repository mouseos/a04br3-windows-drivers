/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_HWRT_PROFILE_H
#define A04_M111_NATIVE_HWRT_PROFILE_H
#include "native_resources.h"
/* RGXAddRenderTarget single RT / 1x1 samples, fixed 800x600 or 800x1280.
 * Consumes the cold owner's actual storage layouts. No allocation, FPU,
 * device writes, record creation, or queue publication. Output has two HWRT
 * inputs for a04_vendor_native_bind_bank_cold. Failure changes nothing. */
int a04_m111_build_single_rt_hwrt_profile(struct a04_m111_hwrt_input out[2],
    uint32_t width,uint32_t height,const struct a04_m111_target_storage *target,
    const struct a04_m111_mlist_storage *mlists);
#endif
