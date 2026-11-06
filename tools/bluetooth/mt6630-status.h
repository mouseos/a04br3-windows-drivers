/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Shared fixed-width ABI. Include after ntddk.h or windows.h + winioctl.h.
 * Snapshot only: no HCI, firmware, MMIO or path arguments. */
#ifndef A04_MT6630_STATUS_H
#define A04_MT6630_STATUS_H
#define MT6630_STATUS_VERSION 1u
#define MT6630_STATUS_PATH L"\\\\.\\MT6630BluetoothStatus"
#define MT6630_QUERY_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN,0x930,METHOD_BUFFERED,FILE_READ_ACCESS)
#define MT6630_STATUS_UNKNOWN 0xffffffffu
enum {MT6630_STATE_LOADED=0,MT6630_STATE_STARTING=1,MT6630_STATE_START_DONE=2,
 MT6630_STATE_TRANSPORT_FAILED=3,MT6630_STATE_STOPPED=4,MT6630_STATE_REMOVED=5,MT6630_STATE_PREPARE_FAILED=6};
typedef struct _MT6630_STATUS {
 ULONG size,version,sequence,state,attempts,stage,start_status,transport_status,running;
 ULONG read_version_sent,read_version_complete,read_version_status,read_version_length,version_valid;
 ULONG hci_version,hci_revision,lmp_version,manufacturer,lmp_subversion;
 ULONG exclusive_bt_profile;
} MT6630_STATUS;
typedef char mt6630_status_size_check[(sizeof(MT6630_STATUS)==80)?1:-1];
#endif
