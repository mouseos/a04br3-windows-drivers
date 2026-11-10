/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef MT8167_THERMAL_IOCTL_STATUS_H
#define MT8167_THERMAL_IOCTL_STATUS_H
#include <stdint.h>
/* Diagnostic snapshot only. No calibration words, ADC samples or pointers.
 * GET_STATUS never starts/retries measurement. completion is an NTSTATUS;
 * successfully retrieving this record does not certify a temperature. */
struct mt8167_thermal_ioctl_status_v1 {
 uint32_t version,bytes,attempted,phase;
 int32_t completion,cal_status,query_status,measure_status,uart_status;
 uint32_t cal_bytes,cal_attributes,parent_stage,owner_active;
 uint32_t request_code,input_bytes,output_bytes;
};
#endif
