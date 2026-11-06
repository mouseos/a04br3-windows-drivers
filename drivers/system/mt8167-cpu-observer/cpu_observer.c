/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "cpu_observer.h"
int a04_cpu_observe(const struct a04_cpu_snapshot *a, const struct a04_cpu_snapshot *b,
                    int devinfo_valid, uint32_t devinfo3, struct a04_cpu_observation *out)
{
 uint32_t pcw, code;
 if (!a || !b || !out) return -1;
 out->valid=0; out->configured_pll_hz=0; out->vproc_uv=0; out->devinfo_bin=0;
 /* Reject the whole sample on any mismatch: clock and PMIC fields are not atomic. */
 if (a->mux!=b->mux || a->pll0!=b->pll0 || a->pll1!=b->pll1 ||
     a->pll_power!=b->pll_power || a->vproc_ctrl!=b->vproc_ctrl ||
     a->vproc_en!=b->vproc_en || a->vosel!=b->vosel || a->vosel_on!=b->vosel_on)
  return -1;
 pcw=a->pll1 & 0x1fffffu;
 /* Only decode the board's known direct ARMPLL path, powered and de-isolated.
  * This is a configured PLL rate, not a measurement of CPU execution rate. */
 if (((a->mux >> 2)&3u)==1u && (a->pll0&1u) &&
     (a->pll_power&3u)==1u && ((a->pll1>>24)&7u)==0u && pcw && pcw<=0xc8000u) {
  out->configured_pll_hz=(uint32_t)(((uint64_t)26000000u*pcw)>>14);
  out->valid|=A04_CPU_PLL_VALID;
 }
 /* MT6392 regulator: selector 0..127, 700 mV + 6.25 mV per step.
  * This is the programmed selector, not a rail-voltage measurement. */
 if (a->vproc_en&1u) {
  code=((a->vproc_ctrl&2u)?a->vosel_on:a->vosel)&0x7fu;
  out->vproc_uv=700000u+6250u*code; out->valid|=A04_CPU_VPROC_VALID;
 }
 if (devinfo_valid && devinfo3!=0xffffffffu) {
  code=devinfo3&7u;
  /* Match only bins actually handled by this Linux driver. */
  if (code<=2u) {out->devinfo_bin=code; out->valid|=A04_CPU_BIN_VALID;}
 }
 return 0;
}
