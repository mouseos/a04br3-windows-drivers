/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Kernel-only software admission. No hardware request, payload or new lease.
 * Include ntddk.h/broker_abi.h first. Existing IOCTL/layouts stay unchanged. */
#ifndef A04_MT6630_ADMISSION_STATUS_H
#define A04_MT6630_ADMISSION_STATUS_H
#define A04_BT_BROKER_ADMISSION CTL_CODE(FILE_DEVICE_UNKNOWN,0x94a,METHOD_BUFFERED,FILE_READ_DATA|FILE_WRITE_DATA)
struct a04_bt_admission_status {ULONG version,bytes,generation,ready,power,initialized,failed,uncertain;};
#endif
