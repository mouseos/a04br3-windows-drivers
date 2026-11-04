/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MSDC2_SDIO_PIO_H
#define A04_MSDC2_SDIO_PIO_H
#include <stddef.h>
#include <stdint.h>
#include "ownership.h"
/* Caller serializes I/O, removal and lease transitions; callbacks provide
 * ordered MMIO and bounded delay and must never outlive the mapping. The
 * controller, card, clock and 512-byte block size must already be configured.
 * No mapping, power, GPIO, interrupt or recovery operations are supplied. */
struct a04_msdc2_pio {
 void *context;
 uint32_t (*read)(void *, unsigned, unsigned);
 void (*write)(void *, unsigned, uint32_t, unsigned);
 void (*delay)(void *, unsigned);
 struct a04_wifi_owner *owner;
 unsigned generation;
 int poisoned;
};
/* Callback compatible with a04_sdio_bus.request. On any submitted command
 * failure, ownership is poisoned and cannot be reused without external recovery. */
int a04_msdc2_cmd53(void *, uint32_t, unsigned, uint8_t *, size_t);
/* Callback for a04_sdio_direct.request; R5 is checked by the caller. */
int a04_msdc2_cmd52(void *, uint32_t, uint32_t *);
/* Restricted control commands: 0, 3, 5, 7, 52 only. */
int a04_msdc2_command(void *, unsigned opcode, uint32_t argument,
                      unsigned response_type, uint32_t *response);
#endif
