/* SPDX-License-Identifier: MIT
 * Board libGLESv2_mtk.so: 0x2025c4, 0x2047e4, 0x202600.
 * Ordinary CPU shadows only. No GPU instruction semantics are guessed. */
#include "native_clear_pds.h"
static void le32(void *cpu,size_t offset,uint32_t value)
{
    unsigned i;uint8_t *p=(uint8_t *)cpu+offset;
    for(i=0;i<4;i++)p[i]=(uint8_t)(value>>(8*i));
}
static int task(uint64_t usc,uint32_t temps,uint64_t *out)
{
    const uint64_t base=UINT64_C(0xba00000000);
    if(usc<base || usc-base>=UINT64_C(0x40000000) || (usc&3u) || temps>252u)return 1;
    /* Assembly: ((temps << 33) + 0x600000000) & 0x1f800000000.
     * Restrict the count before quantization so it cannot wrap the field. */
    *out=(usc-base)|((uint64_t)((temps+3u)/4u)<<35);
    return 0;
}
int a04_m111_patch_background_pds(void *cpu,size_t bytes,uint64_t usc,uint32_t temps)
{
    uint64_t value;
    if(!cpu || bytes<A04_M111_BACKGROUND_PDS_BYTES || task(usc,temps,&value))return 1;
    le32(cpu,0,(uint32_t)value);le32(cpu,4,(uint32_t)(value>>32));
    /* Vendor STP writes 8 bytes but advances the code pointer by 32 bytes.
     * Data words 2..7 are not initialized by that generator: preserve them. */
    le32(cpu,32,0xf4000002u);
    return 0;
}
int a04_m111_build_eot_task_data(void *cpu,size_t bytes,uint64_t usc,uint32_t temps)
{
    uint64_t value;
    if(!cpu || bytes<8u || task(usc,temps,&value))return 1;
    le32(cpu,0,(uint32_t)value);le32(cpu,4,(uint32_t)(value>>32));
    return 0;
}
