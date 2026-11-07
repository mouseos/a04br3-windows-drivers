/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_HI556_CAPTURE_H
#define A04_HI556_CAPTURE_H
#include <stdint.h>
#define A04_HI556_WIDTH 1296u
#define A04_HI556_HEIGHT 972u
#define A04_HI556_STRIDE 1630u
#define A04_HI556_FRAME_BYTES (A04_HI556_STRIDE*A04_HI556_HEIGHT)
enum {A04_CAM_ARGUMENT=-1,A04_CAM_NOT_LIVE=-2,A04_CAM_MMU_UNASSIGNED=-3,
 A04_CAM_RESET_TIMEOUT=-4,A04_CAM_READBACK=-5,A04_CAM_DELAY=-6,A04_CAM_STALE_STATUS=-7,A04_CAM_HARDWARE_ERROR=-8};
/* Fixed model/format only. No arbitrary register/address/program interface. */
struct a04_hi556_receiver_io {
 void *context;
 uint32_t (*read)(void *,unsigned region,unsigned offset);
 void (*write)(void *,unsigned region,unsigned offset,uint32_t);
 int (*delay_us)(void *,unsigned);
 int (*live)(void *);
};
struct a04_hi556_capture_state {
 unsigned stage,armed,quiesced,uncertain,pass1,imgo,changed,guard_errors,gated;
 uint32_t int_status,dma_status,imgo_error,ncsi_status,mux_status,mux_count,tg_count,sof_start,sof_end;
};
/* regions:0=15004000 ISP+SENINF,1=10011000 analog page,2=15001000 LARB1.
 * Common-buffer logical address must be DMA-adapter-provided, below4GiB.
 * Poll mode leaves the assigned IRQ source masked; no fake ISR completion. */
int a04_hi556_receiver_arm(const struct a04_hi556_receiver_io *,struct a04_hi556_capture_state *,uint64_t dma,unsigned bytes);
int a04_hi556_receiver_poll(const struct a04_hi556_receiver_io *,struct a04_hi556_capture_state *);
int a04_hi556_receiver_gate(const struct a04_hi556_receiver_io *,struct a04_hi556_capture_state *);
int a04_hi556_receiver_stop(const struct a04_hi556_receiver_io *,struct a04_hi556_capture_state *);
#endif
