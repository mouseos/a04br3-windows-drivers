/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6392_CLASSD_H
#define A04_MT6392_CLASSD_H
#include <stdint.h>
/* Caller holds the one PWRAP lock throughout, including delays. Prerequisite
 * callbacks must verify actual board power, exclusive ownership and a muted
 * output path; the backend must handle SET/CLR semantics, complete each write
 * and poison the common bus on any timeout. No default backend is provided. */
struct a04_mt6392_classd {
    void *context;
    int (*verify_start)(void *);
    int (*verify_quiescent)(void *);
    int (*supports)(void *,uint16_t,uint16_t,uint16_t);
    int (*update_bits)(void *,uint16_t,uint16_t,uint16_t);
    int (*read16)(void *,uint16_t,uint16_t *);
    int (*delay_us)(void *,uint32_t);
    int active,poisoned;
};
int a04_mt6392_classd_start_muted(struct a04_mt6392_classd *);
int a04_mt6392_classd_stop(struct a04_mt6392_classd *);
#endif
