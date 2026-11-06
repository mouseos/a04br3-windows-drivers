/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WIFI_BOARD_SEQUENCE_H
#define A04_WIFI_BOARD_SEQUENCE_H
#include <stdint.h>
/* The caller must own and serialize TOPCKGEN, GPIO, MSDC2 and the whole
 * MT6630 combo, have verified that RTC 32 kHz remains present, and have
 * quiesced both host and external interrupts. Do not call from an ISR.
 * Any I/O failure poisons the sequence; the caller retains all resources. */
enum a04_wifi_board_bank { A04_WIFI_TOP, A04_WIFI_GPIO, A04_WIFI_MSDC };
struct a04_wifi_board_io {
 void *context;
 int (*qualified)(void *);
 int (*read)(void *,enum a04_wifi_board_bank,unsigned,uint32_t *);
 int (*write)(void *,enum a04_wifi_board_bank,unsigned,uint32_t);
 int (*delay_ms)(void *,unsigned);
 /* Parent-serialized read/modify/write for shared TOP and GPIO. */
 int (*update_shared)(void *,enum a04_wifi_board_bank,unsigned,uint32_t,uint32_t);
};
struct a04_wifi_board { unsigned started,ready,poisoned; };
int a04_wifi_board_start(struct a04_wifi_board *,const struct a04_wifi_board_io *);
/* Request reset and PMU low after IRQ/I/O drain under the same live lease.
 * Success means writes accepted, not physical power-off or firmware stop. */
int a04_wifi_board_poweroff_signals(struct a04_wifi_board *,const struct a04_wifi_board_io *);
#endif
