/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_CAPTURE_STATUS_H
#define A04_CAPTURE_STATUS_H
#include <stdint.h>
#define A04_CAPTURE_READ_STATUS 0x00226048u /* FILE_DEVICE_UNKNOWN, read, buffered */
struct a04_capture_status {
 uint32_t bytes,schema,valid,online,leased,running,fault,irq_events;
 uint32_t dma_base,dma_bytes,position_bytes,latency_status;
};
#endif
