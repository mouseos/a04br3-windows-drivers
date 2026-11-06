/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_TERMINATE_STREAM_H
#define A04_M111_NATIVE_TERMINATE_STREAM_H
#include <stdint.h>
#define A04_M111_TERMINATE_BYTES 76u
#define A04_M111_TERMINATE_PPP_OFFSET 64u
struct a04_m111_terminate_stream { uint8_t data[A04_M111_TERMINATE_BYTES]; };
/* Native OutputTerminateState's no-streamout/no-pending-streamout leaf.
 * page_gpu must name a caller-owned, mapped 4KiB general-data page.
 * VDM at page+0, PPP at page+64; do not put termination in last64 of a page.
 * screen_xy_max is the EXACT native surface+0x150 word from AddRT maxima;
 * do not substitute HWRT ppp_screen or assume modern XML pixel units.
 * This is a suffix builder, not evidence an otherwise empty TA stream suffices.
 * No publication; output and resource lifetime belong to the caller. */
int a04_m111_build_terminate_stream(struct a04_m111_terminate_stream *out,
    uint64_t page_gpu,uint32_t screen_xy_max);
/* Single-sample AddRT dimensions, same no-streamout conditions as above.
 * Uses the board's ceil(/16)-1 maxima, not generic /32 XML units. */
int a04_m111_build_clear_terminate_stream(struct a04_m111_terminate_stream *out,
    uint64_t page_gpu,uint32_t width,uint32_t height);
#endif
