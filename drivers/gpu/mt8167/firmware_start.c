/* SPDX-License-Identifier: MIT
 * GE8300 MIPS wrapper sequence derived from UniXNU-a04br3 NativePowerVR.cpp
 * and Linux PowerVR headers, see fwif/SOURCE. The caller must establish GPU
 * resource ownership, DMA synchronization and correct MMIO ordering first. */
#include "firmware.h"
#include "pvr_rogue_defs.h"
#include "pvr_rogue_mips.h"
#define GPU_REG_BASE UINT64_C(0x13000000)
#define R32(r) io->read32(io->context,(r))
#define R64(r) io->read64(io->context,(r))
#define W32(r,v) io->write32(io->context,(r),(v))
#define W64(r,v) io->write64(io->context,(r),(v))
enum a04_pvr_fw_result a04_pvr_mips_start(const struct a04_pvr_register_io *io,
        uint64_t bvnc, const struct a04_pvr_mips_addresses *a)
{
    uint64_t reset,bypass,remap=ROGUE_MIPSFW_BOOT_REMAP_LOG2_SEGMENT_SIZE;
    if (!io || !a || !io->read32 || !io->read64 || !io->write32 || !io->write64 || !io->delay_us ||
        bvnc!=A04_PVR_BVNC || !a->boot_code || !a->boot_data || !a->exception_code ||
        ((a->boot_code|a->boot_data|a->exception_code)&4095u)) return A04_PVR_FW_INVALID;
    reset=ROGUE_CR_SOFT_RESET__PBE2_XE__MASKFULL;
    W32(ROGUE_CR_SYS_BUS_SECURE,0); (void)R32(ROGUE_CR_SYS_BUS_SECURE);
    W64(ROGUE_CR_SOFT_RESET,reset); (void)R64(ROGUE_CR_SOFT_RESET);
    W64(ROGUE_CR_SOFT_RESET,reset ^ ROGUE_CR_SOFT_RESET_RASCALDUSTS_EN); (void)R64(ROGUE_CR_SOFT_RESET);
    W64(ROGUE_CR_SOFT_RESET,ROGUE_CR_SOFT_RESET_GARTEN_EN); (void)R64(ROGUE_CR_SOFT_RESET);
    bypass=R64(ROGUE_CR_SLC_CTRL_BYPASS) | ROGUE_CR_SLC_CTRL_BYPASS_REQ_IPF_OBJ_EN |
           ROGUE_CR_SLC_CTRL_BYPASS_REQ_IPF_CPF_EN | ROGUE_CR_SLC_CTRL_BYPASS_REQ_TPU_EN;
    W64(ROGUE_CR_SLC_CTRL_BYPASS,bypass); (void)R64(ROGUE_CR_SLC_CTRL_BYPASS);
    W32(ROGUE_CR_SLC_CTRL_MISC,(R32(ROGUE_CR_SLC_CTRL_MISC) & ROGUE_CR_SLC_CTRL_MISC_ENABLE_PSG_HAZARD_CHECK_EN) |
        ROGUE_CR_SLC_CTRL_MISC_ADDR_DECODE_MODE_PVR_HASH1 | ROGUE_CR_SLC_CTRL_MISC_BYPASS_BURST_COMBINER_EN);
    W64(ROGUE_CR_MIPS_WRAPPER_CONFIG,(GPU_REG_BASE >> ROGUE_MIPSFW_WRAPPER_CONFIG_REGBANK_ADDR_ALIGN) |
        ROGUE_CR_MIPS_WRAPPER_CONFIG_BOOT_ISA_MODE_MICROMIPS);
    W64(ROGUE_CR_MIPS_ADDR_REMAP1_CONFIG1,ROGUE_MIPSFW_BOOT_REMAP_PHYS_ADDR_IN | ROGUE_CR_MIPS_ADDR_REMAP1_CONFIG1_MODE_ENABLE_EN);
    W64(ROGUE_CR_MIPS_ADDR_REMAP1_CONFIG2,((uint64_t)a->boot_code & ~ROGUE_CR_MIPS_ADDR_REMAP1_CONFIG2_ADDR_OUT_CLRMSK) | remap);
    W64(ROGUE_CR_MIPS_ADDR_REMAP5_CONFIG1,ROGUE_CR_MIPS_ADDR_REMAP5_CONFIG1_MODE_ENABLE_EN);
    W64(ROGUE_CR_MIPS_ADDR_REMAP5_CONFIG2,((uint64_t)a->boot_code & ~ROGUE_CR_MIPS_ADDR_REMAP5_CONFIG2_ADDR_OUT_CLRMSK) | remap);
    W64(ROGUE_CR_MIPS_ADDR_REMAP2_CONFIG1,ROGUE_MIPSFW_DATA_REMAP_PHYS_ADDR_IN | ROGUE_CR_MIPS_ADDR_REMAP2_CONFIG1_MODE_ENABLE_EN);
    W64(ROGUE_CR_MIPS_ADDR_REMAP2_CONFIG2,((uint64_t)a->boot_data & ~ROGUE_CR_MIPS_ADDR_REMAP2_CONFIG2_ADDR_OUT_CLRMSK) | remap);
    W64(ROGUE_CR_MIPS_ADDR_REMAP3_CONFIG1,ROGUE_MIPSFW_CODE_REMAP_PHYS_ADDR_IN | ROGUE_CR_MIPS_ADDR_REMAP3_CONFIG1_MODE_ENABLE_EN);
    W64(ROGUE_CR_MIPS_ADDR_REMAP3_CONFIG2,((uint64_t)a->exception_code & ~ROGUE_CR_MIPS_ADDR_REMAP3_CONFIG2_ADDR_OUT_CLRMSK) | remap);
    W64(ROGUE_CR_MTS_GARTEN_WRAPPER_CONFIG,ROGUE_CR_MTS_GARTEN_WRAPPER_CONFIG_IDLE_CTRL_META);
    W32(ROGUE_CR_MIPS_DEBUG_CONFIG,0);
    W64(ROGUE_CR_AXI_ACE_LITE_CONFIGURATION,
        (3ULL<<ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_NON_SNOOPING_SHIFT) |
        (3ULL<<ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_NON_SNOOPING_SHIFT) |
        (2ULL<<ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_CACHE_MAINTENANCE_SHIFT) |
        (2ULL<<ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_COHERENT_SHIFT) |
        (2ULL<<ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_COHERENT_SHIFT) |
        (2ULL<<ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_AWCACHE_COHERENT_SHIFT) |
        (2ULL<<ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_COHERENT_SHIFT) |
        (2ULL<<ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_CACHE_MAINTENANCE_SHIFT));
    io->delay_us(io->context,3);
    W64(ROGUE_CR_SOFT_RESET,0); (void)R64(ROGUE_CR_SOFT_RESET);
    io->delay_us(io->context,3);
    return A04_PVR_FW_OK;
}

/* Based on pvr_fw_stop() in the upstream MIT-licensed Linux source. The
 * firmware must already have received a power-off request. Timeout leaves
 * the processor and all allocations owned by the caller. */
static int poll32(const struct a04_pvr_register_io *io,uint32_t reg,uint32_t mask,uint32_t want,struct a04_pvr_stop_failure *failure)
{
    unsigned i; uint32_t value=0;
    for (i=0;i<10000u;++i) {
        value=R32(reg);
        if ((value&mask)==want) return 1;
        io->delay_us(io->context,100);
    }
    if (failure) { failure->reg=reg; failure->value=value; }
    return 0;
}
static int poll64(const struct a04_pvr_register_io *io,uint32_t reg,uint64_t mask,uint64_t want,struct a04_pvr_stop_failure *failure)
{
    unsigned i; uint64_t value=0;
    for (i=0;i<10000u;++i) {
        value=R64(reg);
        if ((value&mask)==want) return 1;
        io->delay_us(io->context,100);
    }
    if (failure) { failure->reg=reg; failure->value=value; }
    return 0;
}
enum a04_pvr_fw_result a04_pvr_mips_stop_report(const struct a04_pvr_register_io *io,uint64_t bvnc,struct a04_pvr_stop_failure *failure)
{
    const uint32_t idle=(uint32_t)ROGUE_CR_SIDEKICK_IDLE_MASKFULL &
        ~(ROGUE_CR_SIDEKICK_IDLE_GARTEN_EN|ROGUE_CR_SIDEKICK_IDLE_SOCIF_EN|ROGUE_CR_SIDEKICK_IDLE_HOSTIF_EN);
    uint64_t reset=ROGUE_CR_SOFT_RESET__PBE2_XE__MASKFULL;
    if (!io || !io->read32 || !io->read64 || !io->write32 || !io->write64 || !io->delay_us || bvnc!=A04_PVR_BVNC)
        return A04_PVR_FW_INVALID;
    if (failure) { failure->reg=0; failure->value=0; }
    if (!poll32(io,ROGUE_CR_SIDEKICK_IDLE,idle,idle,failure) ||
        !poll32(io,ROGUE_CR_SLC_IDLE,(uint32_t)ROGUE_CR_SLC_IDLE_MASKFULL,(uint32_t)ROGUE_CR_SLC_IDLE_MASKFULL,failure))
        return A04_PVR_FW_TIMEOUT;
    W32(ROGUE_CR_MTS_INTCTX_THREAD0_DM_ASSOC,(uint32_t)(ROGUE_CR_MTS_INTCTX_THREAD0_DM_ASSOC_MASKFULL & ROGUE_CR_MTS_INTCTX_THREAD0_DM_ASSOC_DM_ASSOC_CLRMSK));
    W32(ROGUE_CR_MTS_BGCTX_THREAD0_DM_ASSOC,(uint32_t)(ROGUE_CR_MTS_BGCTX_THREAD0_DM_ASSOC_MASKFULL & ROGUE_CR_MTS_BGCTX_THREAD0_DM_ASSOC_DM_ASSOC_CLRMSK));
    if (!poll32(io,ROGUE_CR_BIF_STATUS_MMU,(uint32_t)ROGUE_CR_BIF_STATUS_MMU_MASKFULL,0,failure) ||
        !poll32(io,ROGUE_CR_BIFPM_STATUS_MMU,(uint32_t)ROGUE_CR_BIFPM_STATUS_MMU_MASKFULL,0,failure) ||
        !poll32(io,ROGUE_CR_BIF_READS_EXT_STATUS,(uint32_t)ROGUE_CR_BIF_READS_EXT_STATUS_MASKFULL,0,failure) ||
        !poll32(io,ROGUE_CR_BIFPM_READS_EXT_STATUS,(uint32_t)ROGUE_CR_BIFPM_READS_EXT_STATUS_MASKFULL,0,failure) ||
        !poll64(io,ROGUE_CR_SLC_STATUS1,ROGUE_CR_SLC_STATUS1_MASKFULL,0,failure) ||
        !poll32(io,ROGUE_CR_SLC_IDLE,(uint32_t)ROGUE_CR_SLC_IDLE_MASKFULL,(uint32_t)ROGUE_CR_SLC_IDLE_MASKFULL,failure) ||
        !poll32(io,ROGUE_CR_SIDEKICK_IDLE,idle,idle,failure) ||
        !poll32(io,ROGUE_CR_SIDEKICK_IDLE,ROGUE_CR_SIDEKICK_IDLE_GARTEN_EN,ROGUE_CR_SIDEKICK_IDLE_GARTEN_EN,failure))
        return A04_PVR_FW_TIMEOUT;
    W64(ROGUE_CR_SOFT_RESET,reset);
    { uint64_t value=R64(ROGUE_CR_SOFT_RESET);
      /* MASKFULL includes blocks absent on GE8300, whose reset bits read
       * zero. The idle and outstanding-transaction polls above succeeded;
       * require the firmware processor's implemented reset bit to stick. */
      if (value&ROGUE_CR_SOFT_RESET_GARTEN_EN) return A04_PVR_FW_OK;
      if (failure) { failure->reg=ROGUE_CR_SOFT_RESET; failure->value=value; }
      return A04_PVR_FW_TIMEOUT; }
}
enum a04_pvr_fw_result a04_pvr_mips_stop(const struct a04_pvr_register_io *io,uint64_t bvnc)
{ return a04_pvr_mips_stop_report(io,bvnc,0); }
