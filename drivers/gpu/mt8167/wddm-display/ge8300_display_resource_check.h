/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_DISPLAY_RESOURCE_CHECK_H
#define A04_DISPLAY_RESOURCE_CHECK_H
#include <stdint.h>
/* MT8167 DTS disp_ovl0: 0x14007000 + 0x1000, GIC SPI 160. Only accept
 * resources translated and assigned by PnP; the constant identifies, never
 * authorizes mapping. An absent IRQ allows resource discovery only; frame
 * synchronization remains unavailable. Reject ambiguous/shared resources. */
static int a04_display_resources_valid(unsigned matches, unsigned interrupts,
    int memory_exclusive, int interrupt_exclusive, uint64_t start, uint32_t length)
{
    return matches==1 && interrupts<=1 && memory_exclusive && (!interrupts || interrupt_exclusive) &&
        start==UINT64_C(0x14007000) && length>=0x1000u;
}
#endif
