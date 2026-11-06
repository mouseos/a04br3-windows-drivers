/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_CPU_IOCTL_H
#define A04_CPU_IOCTL_H
#include <stdint.h>
#define A04_CPU_DEVICE L"\\Device\\A04BR3CpuObserve"
#define A04_CPU_LINK L"\\DosDevices\\A04BR3CpuObserve"
#define A04_CPU_USER L"\\\\.\\A04BR3CpuObserve"
#define A04_CPU_GET CTL_CODE(FILE_DEVICE_UNKNOWN,0xa05,METHOD_BUFFERED,FILE_READ_DATA)
/* Fixed width ABI, all rates/selectors are programmed values. */
struct a04_cpu_response {
 uint32_t version, valid, configured_pll_hz, vproc_uv, devinfo_bin;
 uint32_t mux,pll0,pll1,pll_power,vproc_ctrl,vproc_en,vosel,vosel_on;
};
#endif
