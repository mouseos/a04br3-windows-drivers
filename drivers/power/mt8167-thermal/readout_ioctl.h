/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_THERMAL_READOUT_IOCTL_H
#define A04_THERMAL_READOUT_IOCTL_H
/* Include Windows/WDK I/O control and GUID definitions before this header.
 * Output is struct a04_thermal_readout_v1 from readout.h, in millidegrees C. */
#define A04_THERMAL_GET_READING CTL_CODE(FILE_DEVICE_UNKNOWN,0xa04,METHOD_BUFFERED,FILE_READ_DATA)
#define MT8167_THERMAL_GET_IOCTL_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN,0xa05,METHOD_BUFFERED,FILE_READ_DATA)
static const GUID A04ThermalReadoutInterfaceGuid =
 {0x6b2c65ed,0xe2a7,0x4ddc,{0x86,0xba,0x38,0x42,0xe2,0x21,0x4e,0x29}};
#endif
