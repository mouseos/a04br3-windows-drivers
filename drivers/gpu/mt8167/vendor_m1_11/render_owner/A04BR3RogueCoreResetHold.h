/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * GPU0-only hold-reset candidate. Same m1.11 non-S7/nonshared-SLC reset mask
 * as validated startup; RGXSoftReset/RGXStart fence reset writes by reading
 * SOFT_RESET. This observes a reset hold, NOT external AXI write completion,
 * DMA release permission, OS IRQ rundown or GEO_PROOF_DMA_STOPPED. */
#ifndef A04BR3_ROGUE_CORE_RESET_HOLD_H
#define A04BR3_ROGUE_CORE_RESET_HOLD_H
#include "A04BR3RogueStop.h"
#define A04_RGX_CORE_RESET_ALL UINT64_C(0x00e7fffffffffc1d)
/* Fixed 22.40.54.30 / m1.11 config: MIPS, one cluster -> one DUST,
 * one private SLC bank. These implemented top-level reset controls must read
 * asserted: MMU, GARTEN(MIPS), RASCAL, DUST_A and SLC. MASKFULL is a family
 * write mask, not a readback capability mask for absent DUSTs/other units.
 * Vendor RGXStart uses the SOFT_RESET read as the SOCIF posted-write fence. */
#define A04_RGX_CORE_RESET_REQUIRED (UINT64_C(0x0000000400000000) | \
    UINT64_C(0x0000000100000000) | UINT64_C(0x0000000080000000) | \
    UINT64_C(0x0000000020000000) | UINT64_C(0x0000000008000000))
struct a04_rgx_core_reset_observation {
    uint32_t phase,reset_held,failed_register;
    uint64_t observed_reset,failed_value;
};
static inline int a04_rgx_core_stop_hold_reset(const struct a04_rgx_exec_io *io,
    uint64_t bvnc,int poweroff_ack,int irq_drained,uint32_t count,uint32_t delay,
    struct a04_rgx_core_reset_observation *out)
{
    struct a04_rgx_stop_failure f={0};int e;
    if(!out)return A04_EXEC_INPUT;
    *out=(struct a04_rgx_core_reset_observation){0};
    if(!io || !io->read32 || !io->read64 || !io->write32 || !io->write64 ||
       !io->wait_us || bvnc!=UINT64_C(0x001600280036001e) ||
       poweroff_ack!=1 || irq_drained!=1 || !count || !delay ||
       delay>1000 || count>1000000/delay)return A04_EXEC_INPUT;
    /* IRQ must be masked and read back under the owner's actual IRQ lock
     * before entry. This helper never substitutes a mask for OS rundown. */
    out->phase=1;
    if(io->read32(io->context,0x8a0)!=0){out->failed_register=0x8a0;return A04_EXEC_INPUT;}
    e=a04_rgx_ddk_stop(io,bvnc,1,1,count,delay,&f);if(e)goto failed;
    out->phase=2;
    e=a04_rgx_stop_poll32(io,0x3820,7,0,count,delay,&f);if(e)goto failed;
    /* All pre-reset polls are done BEFORE reset can make idle/status zero.
     * Never reinterpret post-reset cleared counters as drained old traffic. */
    out->phase=3;
    io->write64(io->context,0x100,A04_RGX_CORE_RESET_ALL);
    out->observed_reset=io->read64(io->context,0x100); /* source SOCIF fence */
    if(out->observed_reset==UINT64_MAX){e=A04_EXEC_BUS;f.offset=0x100;f.value=out->observed_reset;goto failed;}
    if((out->observed_reset&A04_RGX_CORE_RESET_REQUIRED)!=A04_RGX_CORE_RESET_REQUIRED){
        e=A04_EXEC_TIMEOUT;f.offset=0x100;f.value=out->observed_reset;goto failed;
    }
    out->phase=4;
    if(io->read32(io->context,0x8a0)!=0){e=A04_EXEC_BUS;f.offset=0x8a0;f.value=io->read32(io->context,0x8a0);goto failed;}
    io->write32(io->context,0x8b0,1);
    e=a04_rgx_stop_poll32(io,0x8a8,1,0,count,delay,&f);if(e)goto failed;
    /* Remain in reset. No deassert, power/clock/shared-register operation,
     * owned flag clearing or DMA/MDL free is performed. */
    out->reset_held=1;out->phase=5;return A04_EXEC_OK;
failed:
    out->failed_register=f.offset;out->failed_value=f.value;return e;
}
#endif
