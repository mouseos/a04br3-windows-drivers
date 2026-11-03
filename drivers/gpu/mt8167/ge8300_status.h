/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GE8300_STATUS_H
#define A04_GE8300_STATUS_H
#define A04_GE8300_IOCTL_QUERY 0x00226004u
struct a04_ge8300_status {
    unsigned long version, attempted, boot_status, health_status, shutdown_status;
    unsigned long bvnc_low, bvnc_high, processor_released, firmware_started, queue_faulted;
};
#endif
