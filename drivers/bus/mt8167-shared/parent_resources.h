/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_PARENT_RESOURCES_H
#define A04_PARENT_RESOURCES_H
#include <stdint.h>
#include <stddef.h>
/* Validate raw, exclusive PnP memory resources before mapping. This is
 * a candidate parent layout, not proof that other clients were migrated. */
struct a04_parent_resource { uint64_t base; uint32_t length; unsigned exclusive; };
struct a04_parent_mapping { uint64_t topck, infra, gpio, pwrap; };
int a04_parent_validate_resources(const struct a04_parent_resource *,size_t,
                                  struct a04_parent_mapping *);
int a04_parent_translate_resources(const struct a04_parent_resource *raw,
    const struct a04_parent_resource *translated,size_t count,struct a04_parent_mapping *out);
/* Optional diagnostic: legacy 0x710 or extended 0x714 at 0x10018000.
 * Only the extended range contains the complete 32-bit register at 0x710.
 * It is shared with non-audio users: PnP exclusivity is necessary, not
 * proof of firmware handoff or clock ownership. Output unchanged on error. */
int a04_parent_translate_audio_resources(const struct a04_parent_resource *,
    const struct a04_parent_resource *,size_t,struct a04_parent_mapping *,uint64_t *);
int a04_parent_translate_gpu_domain_resources(const struct a04_parent_resource *,
    const struct a04_parent_resource *,size_t,struct a04_parent_mapping *,uint64_t *,uint64_t *);
#endif
