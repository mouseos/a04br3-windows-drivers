/* SPDX-License-Identifier: MIT
 * MT8167 PIO register read, adapted from the MIT LC709203F transport.
 */
#include "mt8167_i2c.h"
#include "../../bus/mt8167-i2c/pio.h"
int a04_mma_i2c_read(void *context, uint8_t address, uint8_t reg, uint8_t *out, unsigned len)
{
    struct a04_mma_i2c *a = context; const struct a04_i2c_io *io;
    uint8_t data[7]; uint64_t start, now; unsigned i, n; uint16_t status;
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
        now=a->now_ns(io->context); if (now<start || now-start>=10000000u) break;
        status=io->read16(io->context,0x0c);
        now=a->now_ns(io->context); if (now<start || now-start>=10000000u) break;
        if (status&6u) break;
        if (status&1u) {
            if (((io->read16(io->context,0x30)>>4)&15u)!=len) break;
            for (i=0;i<len;++i) data[i]=(uint8_t)io->read16(io->context,0x00);
            io->write16(io->context,0x0c,7);
            for (i=0;i<len;++i) out[i]=data[i];
            return 0;
        }
        io->delay_us(io->context,5);
    }
    io->write16(io->context,0x0c,7); a->faulted=1; return -1;
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
