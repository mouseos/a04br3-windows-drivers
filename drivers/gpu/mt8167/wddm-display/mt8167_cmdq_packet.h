/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_CMDQ_PACKET_H
#define A04_MT8167_CMDQ_PACKET_H
#include <stdint.h>
/* MT8167 DTS values, not the abstract cmdq_event_common.h enum. Linux
 * cmdq_device.c resolves that enum through the per-device event table. */
#define A04_CMDQ_RDMA0_EOF 27u
#define A04_CMDQ_MUTEX0_EOF 38u
#define A04_CMDQ_FLIP_WORDS 8u
#define A04_CMDQ_TWO_LAYER_WORDS 10u
/* Construct an inert command image only. The caller must establish resource,
 * clock, thread and event ownership, freshness, DMA visibility, and prove the
 * active layer/layout before making this image reachable by the GCE. A wait
 * without clear may consume a previously signalled event. No caller may use
 * this function as evidence that a safe boundary exists. L3 is untouched. */
static int a04_mt8167_cmdq_l0_address(uint32_t *words,uint32_t count,
                                     uint32_t event,uint32_t display_address)
{
    if(!words || count<A04_CMDQ_FLIP_WORDS ||
       (event!=A04_CMDQ_RDMA0_EOF && event!=A04_CMDQ_MUTEX0_EOF) ||
       !display_address || (display_address&3u))return 0;
    /* cmdq_record.c WAIT_NO_CLEAR, full-mask WRITE, EOC+IRQ, relative JUMP.
     * DTS mmsys_config_base maps 0x1400xxxx to subsystem 1; subsystem LSB 16.
     * DISP_REG_OVL_L0_ADDR is 0x14007f40. Words are little-endian argB,argA. */
    words[0]=0x00008001u;words[1]=0x20000000u|event;
    words[2]=display_address;words[3]=0x04017f40u;
    words[4]=1u;words[5]=0x40000000u;
    words[6]=8u;words[7]=0x10000000u;
    return 1;
}
/* For an explicitly validated two-layer transaction, write L0 then L3.
 * The caller supplies both currently owned addresses for a separate restore
 * packet. Building either packet does not execute or authorize a switch. */
static int a04_mt8167_cmdq_layer_addresses(uint32_t *words,uint32_t count,
                                          uint32_t event,uint32_t l0,uint32_t l3)
{
    if(!words || count<A04_CMDQ_TWO_LAYER_WORDS ||
       (event!=A04_CMDQ_RDMA0_EOF && event!=A04_CMDQ_MUTEX0_EOF) ||
       !l0 || !l3 || (l0&3u) || (l3&3u))return 0;
    words[0]=0x00008001u;words[1]=0x20000000u|event;
    words[2]=l0;words[3]=0x04017f40u;
    words[4]=l3;words[5]=0x04017fa0u;
    words[6]=1u;words[7]=0x40000000u;
    words[8]=8u;words[9]=0x10000000u;
    return 1;
}
#endif
