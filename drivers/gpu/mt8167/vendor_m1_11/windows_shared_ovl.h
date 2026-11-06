/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_SHARED_OVL_H
#define A04_WINDOWS_SHARED_OVL_H
#include "windows_shared_scanout.h"
#include "../ge8300_status.h"
#ifdef __cplusplus
extern "C" {
#endif
struct a04_shared_ovl { volatile ULONG *base;uint32_t saved;BOOLEAN attempted,restore_attempted; };
/* Callers must supply platform operations establishing device-to-device
 * visibility and complete initialized pixel contents for this particular
 * mapping, plus matching format and an exclusive safe boundary. A validated
 * 32x32 diagnostic alone is not a complete frame.
 * No such production callbacks are implemented. Owner holds display mutex. */
typedef NTSTATUS (*a04_shared_ovl_ready)(void *,const struct a04_shared_scanout *,uint32_t,uint32_t);
typedef NTSTATUS (*a04_shared_ovl_boundary)(void *);
NTSTATUS a04_shared_ovl_switch(struct a04_shared_ovl *,struct a04_shared_scanout *,
    const struct a04_ge8300_shared_lease *,volatile void *,ULONG,uint32_t,
    a04_shared_ovl_ready,a04_shared_ovl_boundary,void *);
NTSTATUS a04_shared_ovl_restore(struct a04_shared_ovl *,struct a04_shared_scanout *,
    a04_shared_ovl_boundary,void *);
#ifdef __cplusplus
}
#endif
#endif
