/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded m1.11 RGXStop register sequence for BVNC 22.40.54.30.
 * Derived from the MIT option of MediaTek/Imagination DDK m1.11ED5425693,
 * rgxstartstop.c RGXStop and hwdefs/km/rgx_cr_defs_km.h. This is not a
 * platform power-off or proof of external fabric drain. Caller must retain
 * DMA ownership until those independent conditions have been established. */
#ifndef A04BR3_ROGUE_STOP_H
#define A04BR3_ROGUE_STOP_H
#include "A04BR3RogueStartupExecutor.h"
struct a04_rgx_stop_failure { uint32_t offset;uint64_t value; };
static inline int a04_rgx_stop_poll32(const struct a04_rgx_exec_io *io,uint32_t off,
    uint32_t mask,uint32_t want,uint32_t count,uint32_t delay,struct a04_rgx_stop_failure *failure)
{
    uint32_t value=0;
    for(uint32_t n=0;n<count;n++) {
        value=io->read32(io->context,off);
        if(value==0xffffffffu) break;
        if((value&mask)==want) return A04_EXEC_OK;
        io->wait_us(io->context,delay);
    }
    if(failure){failure->offset=off;failure->value=value;}return A04_EXEC_TIMEOUT;
}
static inline int a04_rgx_stop_poll64(const struct a04_rgx_exec_io *io,uint32_t off,
    uint64_t mask,uint64_t want,uint32_t count,uint32_t delay,struct a04_rgx_stop_failure *failure)
{
    uint64_t value=0;
    for(uint32_t n=0;n<count;n++) {
        value=io->read64(io->context,off);
        if(value==UINT64_MAX) break;
        if((value&mask)==want) return A04_EXEC_OK;
        io->wait_us(io->context,delay);
    }
    if(failure){failure->offset=off;failure->value=value;}return A04_EXEC_TIMEOUT;
}
static inline int a04_rgx_ddk_stop(const struct a04_rgx_exec_io *io,uint64_t bvnc,
    int poweroff_ack,int irq_drained,uint32_t count,uint32_t delay,
    struct a04_rgx_stop_failure *failure)
{
    uint32_t irq;
    if(!io || !io->read32 || !io->read64 || !io->write32 || !io->wait_us ||
       bvnc!=0x001600280036001eULL || poweroff_ack!=1 || irq_drained!=1 ||
       !count || !delay || delay>1000 || count>1000000/delay) return A04_EXEC_INPUT;
    if(failure){failure->offset=0;failure->value=0;}
    irq=io->read32(io->context,0x8a8);
    if(irq==0xffffffffu) {if(failure){failure->offset=0x8a8;failure->value=irq;}return A04_EXEC_BUS;}
    if(irq&1u) io->write32(io->context,0x8b0,1);
#define STOP32(off,mask,want) do { int e=a04_rgx_stop_poll32(io,(off),(mask),(want),count,delay,failure);if(e)return e; } while(0)
#define STOP64(off,mask,want) do { int e=a04_rgx_stop_poll64(io,(off),(mask),(want),count,delay,failure);if(e)return e; } while(0)
    STOP32(0x3c8,0x78,0x78); /* Sidekick, except Garten/SOCIF/HOSTIF */
    STOP32(0x3898,0xff,0xff); /* SLC idle */
    io->write32(io->context,0xb40,0);io->write32(io->context,0xb30,0);
    io->write32(io->context,0xb48,0);io->write32(io->context,0xb38,0);
    STOP32(0x1358,0xff,0);STOP32(0x1350,0xff,0);
    STOP32(0x1320,0x0fffffff,0);STOP32(0x1338,0xffff,0);
    STOP64(0x3870,UINT64_C(0x800003ff03ffffff),0);
    /* 22.0.54.30 feature table reports one SLC bank: STATUS2 is four-bank only. */
    STOP32(0x3898,0xff,0xff);STOP32(0x3c8,0x78,0x78);STOP32(0x3c8,4,4);
#undef STOP32
#undef STOP64
    return A04_EXEC_OK;
}
#endif
