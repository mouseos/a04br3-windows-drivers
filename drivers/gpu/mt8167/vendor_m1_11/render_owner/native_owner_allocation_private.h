/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_OWNER_ALLOCATION_PRIVATE_H
#define A04_NATIVE_OWNER_ALLOCATION_PRIVATE_H
#include "native_owner_allocation.h"
#include "windows_render_cold.h"
#include "windows_start.h"
#include "native_owner_dispatch.h"
/* Private to provider implementation. H does not need m1.11 FWIF structs. */
struct a04_native_owner_allocation {
    PDEVICE_OBJECT pdo;PDMA_ADAPTER adapter;ULONG map_registers;
    struct a04_native_owner_frontend frontend;
    struct a04_vendor_m1_11_memory firmware;
    struct a04_vendor_render_cold render;
    struct a04_vendor_native_cold *native;
    struct a04_native_mdl_binding *binding,*vertex;
    BOOLEAN binding_borrowed; /* factory resource owns selected RT MDL */
    struct geo_frontend_mdl_lease *frontend_orphan; /* unreleaseable resolver result: retain owner */
    struct a04_vendor_start start;
    struct a04_native_irq_core irq;
    struct a04_native_owner_dispatch dispatch;
    struct a04_vendor_native_diagnostic diagnostic;
};
#endif
