/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_CLEAR_VALIDATE_H
#define A04_VENDOR_CLEAR_VALIDATE_H
#include <stdint.h>
/* Readback covers the entire 64 KiB allocation, including inter-row guards. */
static inline int a04_vendor_clear_layout_valid(uint32_t first,uint32_t row)
{
    return !(first&3u) && !(row&3u) && row>=128u && row<=2048u &&
           first<=65536u-128u && (uint64_t)first+31u*(uint64_t)row+128u<=65536u;
}
static inline int a04_vendor_clear_is_target(uint32_t byte,uint32_t first,uint32_t row)
{
    uint32_t offset;
    if(byte<first)return 0;
    offset=byte-first;
    return offset/row<32u && offset%row<128u;
}
#endif
