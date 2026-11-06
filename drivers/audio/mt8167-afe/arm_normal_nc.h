/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_ARM_NORMAL_NC_H
#define MT8167_ARM_NORMAL_NC_H
#include <stdint.h>
/* Arm DDI 0406C.d B4.1.112 PAR, ATS1CPR stage-1 result, not a Windows PTE layout.
 * Short descriptors: Inner[6:4]=000, Outer[3:2]=00 => Normal NC.
 * Device/Strongly-ordered encodings have a nonzero Inner field.
 * Long descriptors: ATTR[63:56]=0x44 => Normal, Inner/Outer NC.
 * SS results are not accepted: PortCls page allocations need no supersections.
 * expectedPage is page aligned. Never infer a DMA address from PAR. */
static inline int mt8167_arm_par_normal_nc(uint64_t par,uint32_t expectedPage)
{
    if(par & 1u)return 0; /* translation fault */
    if(par & (1u<<11)) {
        return (par>>56)==0x44u &&
            (par & UINT64_C(0x000000fffffff000))==expectedPage;
    }
    return !(par & 0x7eu) &&
        ((uint32_t)par & 0xfffff000u)==expectedPage;
}
#endif
