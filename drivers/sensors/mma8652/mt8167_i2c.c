/* SPDX-License-Identifier: MIT
 * MT8167 PIO register read, adapted from the MIT LC709203F transport.
 */
#include "mt8167_i2c.h"
#include "../../bus/mt8167-i2c/pio.h"
int a04_mma_i2c_recover_stopped(struct a04_mma_i2c *a)
{
    const struct a04_i2c_io *io;
    uint16_t timing,config,dcm,hs,delay,ext;
    if(!a || !(io=a->io) || !io->read16 || !io->write16 || !io->delay_us ||
       (io->read16(io->context,0x24)&1u)) return -1;
    timing=io->read16(io->context,0x20);
    if(timing&~0x073fu)return -1;
    config=io->read16(io->context,0x40);dcm=io->read16(io->context,0x70);
    hs=io->read16(io->context,0x48);delay=io->read16(io->context,0x28);
    ext=io->read16(io->context,0x60);
    /* MTK v1 SOFTRESET=0x50, as used by Linux mtk_i2c_init_hw after ACK
     * errors/timeouts and the existing XNU MT8167 PIO implementation. The
     * exclusive caller already revalidated HSTA; START must be idle. */
    io->write16(io->context,0x50,1);io->delay_us(io->context,5);
    io->write16(io->context,0x08,0);
    io->write16(io->context,0x0c,7);
    io->write16(io->context,0x38,1);
    io->write16(io->context,0x40,config);io->write16(io->context,0x70,dcm);
    io->write16(io->context,0x20,timing);io->write16(io->context,0x48,hs);
    io->write16(io->context,0x28,delay);io->write16(io->context,0x60,ext);
    io->write16(io->context,0x10,0x28);io->write16(io->context,0x1c,2);
    if(io->read16(io->context,0x24)&1u)return -1;
    a->faulted=0; return 0;
}
int a04_mma_i2c_read(void *context, uint8_t address, uint8_t reg, uint8_t *out, unsigned len)
{
    struct a04_mma_i2c *a = context; const struct a04_i2c_io *io;
    uint8_t data[7]; uint64_t start, now; unsigned i, n; uint16_t status; int reason=2;
    if (!a || !(io=a->io) || !io->read16 || !io->write16 || !io->delay_us ||
        !a->now_ns || a->faulted || address != 0x1d || !out || !len || len>7) return -1;
    if (io->read16(io->context,0x24)&1u) { a->faulted=1; return -1; }
    io->write16(io->context,0x08,0); io->write16(io->context,0x0c,7);
    io->write16(io->context,0x38,1);
    io->write16(io->context,0x10,(uint16_t)((io->read16(io->context,0x10)&~0x3eu)|0x3au));
    io->write16(io->context,0x04,(uint16_t)(address<<1));
    io->write16(io->context,0x14,1); io->write16(io->context,0x6c,(uint16_t)len);
    io->write16(io->context,0x18,2); io->write16(io->context,0x00,reg);
    io->write16(io->context,0x24,1); start=a->now_ns(io->context);
    for (n=0;n<2000;++n) {
        /* A PASSIVE_LEVEL owner can be descheduled after START. Hardware
         * completion must win over a wall-clock deadline in that case. */
        status=io->read16(io->context,0x0c);
        if (status&6u) { reason=3; break; }
        if (status&1u) {
            if (((io->read16(io->context,0x30)>>4)&15u)!=len) { reason=4; break; }
            for (i=0;i<len;++i) data[i]=(uint8_t)io->read16(io->context,0x00);
            io->write16(io->context,0x0c,7);
            for (i=0;i<len;++i) out[i]=data[i];
            return 0;
        }
        now=a->now_ns(io->context);
        if(now<start){reason=5;break;}
        if(now-start>=100000000u)break;
        io->delay_us(io->context,5);
    }
    io->write16(io->context,0x0c,7); a->faulted=reason; return -1;
}
int a04_mma_i2c_write(void *context, uint8_t address, uint8_t reg, uint8_t value)
{
    struct a04_mma_i2c *a=context; uint8_t bytes[2];
    if (!a || !a->io || !a->io->read16 || !a->io->write16 || !a->io->delay_us ||
        a->faulted || address!=0x1d) return -1;
    if (a->io->read16(a->io->context,0x24)&1u) { a->faulted=1; return -1; }
    bytes[0]=reg; bytes[1]=value;
    if (a04_i2c_transfer(a->io,address,0,bytes,2)!=A04_I2C_OK) { a->faulted=1; return -1; }
    return 0;
}
