/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_USB_BQHR_IOCTL_H
#define A04_USB_BQHR_IOCTL_H
#define A04_BQHR_GET CTL_CODE(FILE_DEVICE_UNKNOWN,0x852,METHOD_BUFFERED,FILE_READ_ACCESS)
#define A04_BQHR_DEVICE L"\\Device\\A04UsbBqhr"
#define A04_BQHR_LINK L"\\DosDevices\\A04UsbBqhr"
typedef struct { ULONG size,version,result,status; unsigned char record[128]; } A04_BQHR_RESPONSE;
#endif
