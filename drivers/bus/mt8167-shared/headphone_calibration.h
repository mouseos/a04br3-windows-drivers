/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_HP_CALIBRATION_REPORT_H
#define A04_HP_CALIBRATION_REPORT_H
#include <stdint.h>
/* Actual calibration timing/ADC evidence, NOT a KSRTAUDIO_HWLATENCY claim.
 * No input coefficients, phase labels, MMIO pointers or DMA addresses. */
struct a04_hp_calibration_report {
 uint32_t size,version,attempted,state,generation;int32_t status;
 uint32_t dma_base,dma_bytes,phase_mask,ks_latency_status,stop_observed,same_buffer;
 uint64_t stamp100ns[7]; /* prepare, DMA observed, A, B, C, stopped, ready */
 int32_t uv[2][3];uint16_t raw[2][3][22];int16_t dc_dac[2],dc_op[2];
};
typedef char a04_hp_calibration_report_must_be_400[(sizeof(struct a04_hp_calibration_report)==400)?1:-1];
#define IOCTL_MT8167_HP_CALIBRATION_STATUS 0x00226560u
#endif
