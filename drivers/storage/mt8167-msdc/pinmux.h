/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MSDC_PINMUX_H
#define A04_MSDC_PINMUX_H
#include <stdint.h>
/* PASSIVE_LEVEL only. The parent must already hold an exclusive, generation-
 * bound lease across GPIO, clocks, PMIC, firmware and every other client.
 * update_masked must perform an atomic shared-register update under the real
 * parent lock. No such Windows parent is currently connected. */
struct a04_pinmux_ops {
    void *context;
    int (*held)(void *,uint32_t);
    int (*quiescent_and_powered)(void *,uint32_t);
    int (*read32)(void *,unsigned,uint32_t *);
    int (*update_masked)(void *,unsigned,uint32_t mask,uint32_t value);
    /* Atomic hardware SET/CLR alias: set=1 writes base+4, set=0 base+8. */
    int (*write_alias)(void *,unsigned base,uint32_t mask,int set);
};
struct a04_pinmux_state { unsigned poisoned; };
enum a04_pinmux_result { A04_PINMUX_OK,A04_PINMUX_INVALID,A04_PINMUX_UNOWNED,
    A04_PINMUX_UNSAFE,A04_PINMUX_IO,A04_PINMUX_MISMATCH,A04_PINMUX_POISONED };
/* Sets only the mode fields of GPIO110..120 to 1; does not set pull, drive,
 * clock, or power. Failure poisons the state; partial updates are not undone. */
enum a04_pinmux_result a04_msdc_pinmux_mode(struct a04_pinmux_state *,
    const struct a04_pinmux_ops *,uint32_t generation);
/* Explicit UHS profile from the board DTS. GPIO114 reset bias is deliberately
 * untouched because its resistor selection was not specified. */
enum a04_pinmux_result a04_msdc_pinmux_uhs(struct a04_pinmux_state *,
    const struct a04_pinmux_ops *,uint32_t generation);
#endif
