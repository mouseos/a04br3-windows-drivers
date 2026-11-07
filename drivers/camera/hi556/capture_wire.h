/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_HI556_CAPTURE_WIRE_H
#define A04_HI556_CAPTURE_WIRE_H
#include <stdint.h>
#include "capture.h"
#define A04_CAM_STATUS_VERSION 2u
#define A04_HI556_CAPTURE_GUID_VALUES {0xf3875660,0xa044,0x4001,{0xb1,0x29,0x05,0x56,0x00,0x00,0x00,0x01}}
#define A04_CAM_IOCTL_CAPTURE CTL_CODE(FILE_DEVICE_UNKNOWN,0x856,METHOD_OUT_DIRECT,FILE_READ_DATA)
#define A04_CAM_IOCTL_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN,0x857,METHOD_BUFFERED,FILE_READ_DATA)
struct a04_cam_identity_wire {
 uint32_t stage,bytes_read,id_observed,power_observed,transport_observed,release_observed,quarantined;
 uint16_t chip_id,reserved;
 int32_t transport_status,power_status,power_release_status;
};
typedef struct {
 uint32_t version,bytes,generation,attempted;
 int32_t status,sensor_status,stop_status;
 uint32_t frame_bytes,width,height,stride,format;
 struct a04_cam_identity_wire identity;
 struct a04_hi556_capture_state receiver;
 uint32_t sensor_observed,stop_observed,stop_requested,deadline_expired;
 int32_t receiver_result;uint32_t receiver_observed;
 int32_t qualification_status;uint32_t qualification_observed,power_lost;
} A04_CAM_STATUS;
#endif
