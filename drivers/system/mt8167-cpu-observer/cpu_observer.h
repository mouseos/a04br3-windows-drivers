/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_CPU_OBSERVER_H
#define A04_CPU_OBSERVER_H
#include <stdint.h>
/* Caller supplies two consecutive snapshots under the resource owner's lock.
 * No MMIO/PMIC access, synchronization, or hardware control is done here. */
struct a04_cpu_snapshot {
 uint32_t mux, pll0, pll1, pll_power, vproc_ctrl, vproc_en, vosel, vosel_on;
};
struct a04_cpu_observation {
 uint32_t valid, configured_pll_hz, vproc_uv, devinfo_bin;
};
#define A04_CPU_PLL_VALID 1u
#define A04_CPU_VPROC_VALID 2u
#define A04_CPU_BIN_VALID 4u
enum a04_cpu_field {
 A04_CPU_MUX, A04_CPU_PLL0, A04_CPU_PLL1, A04_CPU_PLL_POWER,
 A04_CPU_VPROC_CTRL, A04_CPU_VPROC_EN, A04_CPU_VOSEL, A04_CPU_VOSEL_ON,
 A04_CPU_FIELD_COUNT
};
typedef int (*a04_cpu_read_field)(void *, enum a04_cpu_field, uint32_t *);
/* devinfo must be obtained from a trusted provider, not inferred from MMIO.
 * Its absence is distinct from Linux's valid efuse bin 0. */
int a04_cpu_observe(const struct a04_cpu_snapshot *, const struct a04_cpu_snapshot *,
                    int devinfo_valid, uint32_t devinfo3,
                    struct a04_cpu_observation *);
/* Caller holds the appropriate owner's lifetime and serialization across this
 * call. Callback returns zero only for a successfully read named field. */
int a04_cpu_capture(a04_cpu_read_field, void *, int, uint32_t,
                    struct a04_cpu_observation *);
#endif
