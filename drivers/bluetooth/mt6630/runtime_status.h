/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_RUNTIME_STATUS_H
#define A04_MT6630_RUNTIME_STATUS_H
#define A04_BT_BROKER_RUNTIME CTL_CODE(FILE_DEVICE_UNKNOWN,0x94b,METHOD_BUFFERED,FILE_READ_DATA)
#define A04_BT_RUNTIME_REQUIRED 0x7ffu
#define A04_BT_RUNTIME_FAULTS 0x1f0000u
struct a04_bt_runtime_status {
 unsigned version,bytes,generation,flags,required_flags,missing_flags;
 int bt_failure,wmt_failure,radio_failure;
 unsigned wmt_commands,wmt_acked,wmt_phase,tx_pending,queued_packets,transport_result,reserved;
};
/* Only software status. No address/key/calibration/payload/PCM coefficients. */
#endif
