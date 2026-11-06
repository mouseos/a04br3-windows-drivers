/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_USB_OBSERVATION_IOCTL_H
#define A04_USB_OBSERVATION_IOCTL_H
/* Include ntddk.h or windows.h/winioctl.h before this header. */
#define A04_USB_OBSERVATION_DEVICE L"\\Device\\A04UsbObservation"
#define A04_USB_OBSERVATION_LINK L"\\DosDevices\\A04UsbObservation"
#define A04_USB_OBSERVATION_GET CTL_CODE(FILE_DEVICE_UNKNOWN,0x850,METHOD_BUFFERED,FILE_READ_ACCESS)
#define A04_USB_OBSERVATION_VERSION 1u
typedef struct _A04_USB_OBSERVATION {
 ULONG size,version;
 ULONG dir1,dout1,din1,mode21_22;
 ULONG dir2,dout2,din2,mode34,mode41;
} A04_USB_OBSERVATION;
#endif
