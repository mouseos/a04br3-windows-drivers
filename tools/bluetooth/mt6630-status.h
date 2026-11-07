/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Shared fixed-width ABI. Include after ntddk.h or windows.h + winioctl.h.
 * Snapshot only: no HCI, firmware, MMIO or path arguments. */
#ifndef A04_MT6630_STATUS_H
#define A04_MT6630_STATUS_H
#define MT6630_STATUS_VERSION 1u
#define MT6630_STATUS_PATH L"\\\\.\\MT6630BluetoothStatus"
#define MT6630_QUERY_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN,0x930,METHOD_BUFFERED,FILE_READ_ACCESS)
#define MT6630_QUERY_BTHX_CONTRACT CTL_CODE(FILE_DEVICE_UNKNOWN,0x931,METHOD_BUFFERED,FILE_READ_ACCESS)
#define MT6630_BTHX_CONTRACT_VERSION 1u
/* Microsoft QUERY_CAPABILITIES Windows 8/8.1 load requirements, not a claim
 * that this controller has the required sideband audio implementation. */
#define MT6630_BTHX_REQUIRED_SCO 2u
#define MT6630_BTHX_REQUIRED_CHANNELS 1u
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
/* Independent extension; the existing 80-byte version snapshot is unchanged.
 * Values describe the last actual kernel BTHX capabilities request, not an
 * assumption based on probe build version or raw controller version. */
typedef struct _MT6630_BTHX_CONTRACT {
 ULONG size,version,queries,returned_capabilities;
 ULONG advertised_sco,advertised_channels,required_sco,required_channels;
 ULONG contract_satisfied,last_query_status;
} MT6630_BTHX_CONTRACT;
typedef char mt6630_contract_size_check[(sizeof(MT6630_BTHX_CONTRACT)==40)?1:-1];
#endif
