/* SPDX-License-Identifier: MIT
 * MT8167 sequence based on UniXNU-a04br3 A04BR3ChargerCore.h (MIT).
 */
#include "mt8167_i2c.h"
static int transfer(struct a04_gauge_mt8167 *a, int read, uint8_t reg, uint8_t *value)
{
    const struct a04_i2c_io *io;
    uint64_t start, now;
    uint16_t status;
    unsigned i;
    if (!a || !(io=a->io) || !io->read16 || !io->write16 || !io->delay_us ||
        !a->monotonic_ns || !value || a->faulted || reg > 7 ||
        (!read && (reg != 0 || (*value & ~0x88u) || !(*value & 0x80u)))) return -1;
    if (io->read16(io->context, 0x24) & 1u) { a->faulted=1; return -1; }
    io->write16(io->context,0x08,0); io->write16(io->context,0x0c,7);
    io->write16(io->context,0x38,1);
    io->write16(io->context,0x10,(uint16_t)((io->read16(io->context,0x10)&~0x3eu)|0x28u|(read?0x12u:0u)));
    io->write16(io->context,0x04,0xd6); io->write16(io->context,0x14,read?1:2);
    io->write16(io->context,0x6c,read?1:0); io->write16(io->context,0x18,read?2:1);
    io->write16(io->context,0,reg); if (!read) io->write16(io->context,0,*value);
    io->write16(io->context,0x24,read?1:0x4001);
    start=a->monotonic_ns(io->context);
    for (i=0;i<2000;++i) {
        now=a->monotonic_ns(io->context);
        if (now<start || now-start>=10000000ULL) break;
        status=io->read16(io->context,0x0c);
        now=a->monotonic_ns(io->context);
        if (now<start || now-start>=10000000ULL || (status&6u)) break;
        if (status&1u) {
            if (read) {
                if (((io->read16(io->context,0x30)>>4)&15u)!=1u) break;
                *value=(uint8_t)io->read16(io->context,0);
            }
            io->write16(io->context,0x0c,7); return 0;
        }
        io->delay_us(io->context,5);
    }
    io->write16(io->context,0x0c,7); a->faulted=1; return -1;
}
int a04_bq_mt8167_read(void *context,uint8_t reg,uint8_t *value)
{ return transfer(context,1,reg,value); }
int a04_bq_mt8167_kick(void *context,uint8_t value)
{ return transfer(context,0,0,&value); }
