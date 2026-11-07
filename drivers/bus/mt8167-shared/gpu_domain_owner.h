/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GPU_DOMAIN_OWNER_H
#define A04_GPU_DOMAIN_OWNER_H
#include <stdint.h>
/* Register facts: MT8167 mtk-scpsys-mt8167.c/mtk-infracfg.c, and XNU
 * f5d43bf A04BR3RoguePower.h. No MT8173 masks, GPUCore map, clock/PLL,
 * watchdog, CPU hotplug or caller-selected register operations.
 * One legitimate parent lock covers the entire bounded operation. */
enum a04_gpu_domain_bank { A04_GPU_DOMAIN_INFRA, A04_GPU_DOMAIN_SPM };
struct a04_gpu_domain_io {
 void *context;
 int (*read)(void *,enum a04_gpu_domain_bank,unsigned,uint32_t *);
 int (*write)(void *,enum a04_gpu_domain_bank,unsigned,uint32_t);
 void (*delay_us)(void *,unsigned);
 unsigned has_spm; /* Set only from actual validated PnP assignment, not intent. */
};
struct a04_gpu_domain_owner {
 uint64_t generation,lease;
 unsigned held,protected_bus,domains_off,poisoned,revoked;
 unsigned failed_bank,failed_offset;uint32_t failed_value;
};
#define A04_GPU_DOMAIN_OK 0
#define A04_GPU_DOMAIN_INPUT (-1)
#define A04_GPU_DOMAIN_BUSY (-2)
#define A04_GPU_DOMAIN_IO (-3)
#define A04_GPU_DOMAIN_TIMEOUT (-4)
#define A04_GPU_DOMAIN_UNASSIGNED (-5)
/* Begin is a hardware readback, not an assertion that DMA has stopped. */
int a04_gpu_domain_acquire(struct a04_gpu_domain_owner *,const struct a04_gpu_domain_io *,uint64_t *);
int a04_gpu_domain_protect(struct a04_gpu_domain_owner *,const struct a04_gpu_domain_io *,uint64_t);
int a04_gpu_domain_unprotect(struct a04_gpu_domain_owner *,const struct a04_gpu_domain_io *,uint64_t);
/* Full off/on are possible ONLY with real assigned SPM. Parent ecaa1008 has
 * no SPM assignment: it must not publish these operations. OFF success is a
 * platform-domain observation, not IRQ/cache/token/GEO DMA_STOPPED proof.
 * Caller closes native admission and finishes FW/IRQ/RGXStop first. */
int a04_gpu_domain_off(struct a04_gpu_domain_owner *,const struct a04_gpu_domain_io *,uint64_t);
int a04_gpu_domain_on(struct a04_gpu_domain_owner *,const struct a04_gpu_domain_io *,uint64_t);
int a04_gpu_domain_release(struct a04_gpu_domain_owner *,uint64_t);
/* Revoke is software-only. Ambiguous/held resources remain pinned. */
void a04_gpu_domain_revoke(struct a04_gpu_domain_owner *);
#endif
