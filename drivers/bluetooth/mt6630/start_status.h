/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_START_STATUS_H
#define A04_MT6630_START_STATUS_H
#include <stdint.h>
/* Software receipts for the existing OPEN only; no payload/identity bytes. */
enum a04_bt_start_stage {A04_BT_START_NONE=0,A04_BT_START_ADMISSION=1,A04_BT_START_NVRAM=2,A04_BT_START_ATTACH=3,A04_BT_START_ADDRESS_VALID=4,A04_BT_START_FUNCTION=5,A04_BT_START_SET_ADDRESS=6,A04_BT_START_PCM=7,A04_BT_START_RADIO=8,A04_BT_START_TX_POWER=9,A04_BT_START_SLEEP=10,A04_BT_START_COEX=11,A04_BT_START_VERSION=12,A04_BT_START_VERSION_DECODE=13,A04_BT_START_READ_ADDRESS=14,A04_BT_START_ADDRESS_COMPARE=15,A04_BT_START_READY=16};
#define A04_BT_START_RETURNED 1u
#define A04_BT_START_BACKEND_VALID 2u
#define A04_BT_START_NT_VALID 4u
struct a04_bt_start_status {uint32_t version,bytes,generation,sequence,entered_mask,returned_mask,succeeded_mask,last_stage,broker_ntstatus;int32_t backend_status;uint32_t last_opcode,flags,start_filetime_low,start_filetime_high,start_interrupttime_low,start_interrupttime_high;};
/* Include broker_abi.h/ntddk.h before using this private kernel-only IOCTL. */
#define A04_BT_BROKER_START_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN,0x949,METHOD_BUFFERED,FILE_READ_DATA|FILE_WRITE_DATA)
struct a04_bt_transport;
typedef void (*a04_bt_start_observer)(void *,unsigned,unsigned,int,unsigned);
int a04_bt_initialize_observed(struct a04_bt_transport *,const uint8_t [36],a04_bt_start_observer,void *);
#endif
