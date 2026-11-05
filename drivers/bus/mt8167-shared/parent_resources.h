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
#endif
