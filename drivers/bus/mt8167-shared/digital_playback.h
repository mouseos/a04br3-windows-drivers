/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_DIGITAL_PLAYBACK_H
#define A04_MT8167_DIGITAL_PLAYBACK_H
#include <stdint.h>
/* Board Linux mt8167-afe-pcm.c: int_adda_startup/hw_params/prepare,
 * O03/O04 DAPM mixers; mt8167-codec.c: setup_dl_rate/hw_params.
 * Caller holds the parent lock, a muted codec lease, exclusive AFE and
 * retained power. This enables the DIGITAL path only, never an analog amp.
 * An attempted write retains ownership until reset: no DMA drain is inferred.
 */
struct a04_digital_io {
 void *context;
 int (*read)(void *,unsigned,uint32_t *);
 int (*write)(void *,unsigned,uint32_t);
 int (*delay_us)(void *,unsigned);
};
struct a04_digital_playback { int retained,prepared,poisoned; };
static inline int a04_digital_update(const struct a04_digital_io *io,unsigned off,
                              uint32_t mask,uint32_t bits)
{
 uint32_t value,after;
 if(io->read(io->context,off,&value) ||
    io->write(io->context,off,(value&~mask)|(bits&mask)) ||
    io->read(io->context,off,&after) || (after&mask)!=(bits&mask))return -1;
 return 0;
}
static inline int a04_digital_prepare(struct a04_digital_playback *s,
                               const struct a04_digital_io *io,unsigned rate)
{
 uint32_t dac,src,adda,ul,i2s,abb,control;
 if(!s || !io || !io->read || !io->write || !io->delay_us ||
    rate!=48000 || s->retained || s->poisoned)return -1;
 /* Read-only admission: do not commandeer inherited active playback/capture.
  * Parent preflight must also exclude all other clients and clock owners. */
 if(io->read(io->context,0x10,&dac) || (dac&0xdeu) ||
    io->read(io->context,0x108,&src) || (src&1u) ||
    io->read(io->context,0x124,&adda) || (adda&1u) ||
    io->read(io->context,0x114,&ul) || (ul&1u) ||
    io->read(io->context,0x34,&i2s) || (i2s&1u) ||
    io->read(io->context,0xf00,&abb) || (abb&3u))return -1;
 s->retained=1;
 /* int_adda_startup: DAC and DAC_PREDIS clock gates; AFE_ON. */
 if(a04_digital_update(io,0, (1u<<25)|(1u<<26),0) ||
    a04_digital_update(io,0x10,1,1) || io->delay_us(io->context,100) ||
    /* S16_LE, DL1 I05/I06 -> O03/O04. Preserve unrelated mixers. */
    a04_digital_update(io,0x6c,0x18,0) ||
    a04_digital_update(io,0x24,1u<<21,1u<<21) ||
    a04_digital_update(io,0x28,1u<<6,1u<<6) ||
    a04_digital_update(io,0x260,0xffffffffu,0) ||
    a04_digital_update(io,0x264,0xffffffffu,0) ||
    a04_digital_update(io,0x108,0xffffffffu,0x83001802u) ||
    a04_digital_update(io,0x10c,0xffffffffu,0xf74f0000u) ||
    /* I2S fs=10 differs from ADDA/codec fs=8 at 48 kHz. */
    a04_digital_update(io,0x34,0xfffffffeu,0x00041a08u) ||
    /* Codec hw_params, rate latch from STATUS, not command bit. */
    a04_digital_update(io,0xf04,15,8) ||
    io->read(io->context,0xf2c,&control) ||
    a04_digital_update(io,0xf2c,0x100,(control&1u)?0:0x100) ||
    a04_digital_update(io,0xf28,1,1) ||
    a04_digital_update(io,0xf0c,0xffff,0) ||
    a04_digital_update(io,0xf10,0xffff,0) ||
    io->read(io->context,0xf2c,&control) ||
    a04_digital_update(io,0xf2c,0x200,(control&2u)?0:0x200) ||
    a04_digital_update(io,0x108,1,1) ||
    a04_digital_update(io,0x124,1,1) ||
    a04_digital_update(io,0x34,1,1)) {s->poisoned=1;return -1;}
 s->prepared=1;return 0;
}
#endif
