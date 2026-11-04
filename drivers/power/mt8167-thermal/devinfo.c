/* SPDX-License-Identifier: GPL-2.0-only
 * MediaTek Linux devinfo.c defines atag,devinfo as a native little-endian
 * u32 count followed by count u32 entries on this ARM32 device. */
#include "devinfo.h"
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
int a04_devinfo_atag_read(void *ctx, unsigned index, uint32_t *out)
{
    const struct a04_devinfo_blob *b=ctx; uint32_t count;
    if (!b || !b->bytes || !out || b->size<4) return -1;
    count=le32(b->bytes);
    if (count>300 || (size_t)count>(b->size-4)/4 || index>=count) return -1;
    *out=le32(b->bytes+4+(size_t)index*4); return 0;
}
