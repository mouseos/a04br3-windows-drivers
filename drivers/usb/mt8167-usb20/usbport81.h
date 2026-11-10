/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_USBPORT81_H
#define A04_USBPORT81_H
#include <ntddk.h>
#include <usb.h>
/* RT81 ARM32 private ABI, bounded to the version-200 packet.
 * Every populated slot is linked to original/metadata-seeded REA evidence in
 * USBPORT81.md. Do not use this layout on another architecture or OS. */
#define A04_MP_OK 0u
#define A04_MP_ERROR 1u
#define A04_MP_RESOURCES 2u
#define A04_MP_UNSUPPORTED 6u
#define A04_MP_HARDWARE 7u
#define A04_EP_PAUSED 2u
#define A04_EP_ACTIVE 3u
#define A04_EP_REMOVE 4u
#define A04_EP_CLOSED 5u
struct a04_mp_resources {
 ULONG types,flavor,vector;UCHAR level,pad0[3];KAFFINITY affinity;
 BOOLEAN shared;UCHAR pad1[3];ULONG mode;PVOID interrupt,base;ULONG bytes;
 UCHAR remainder[0x7c-0x28];PDEVICE_OBJECT fdo,pdo;UCHAR nonpci,pad2[3];PVOID acpi;ULONG acpi_bytes;
};
struct a04_mp_properties {
 USHORT address,endpoint,packet;UCHAR period,reserved0;
 ULONG speed,bandwidth,schedule,type,direction;PVOID common_va;
 ULONG common_pa,common_bytes,reserved1,max_transfer;
 USHORT hub,port;UCHAR int_mask,split_mask,transactions,reserved2;
 ULONG max_packet,reserved3;
};
struct a04_mp_parameters {ULONG flags,bytes,counter,split,reserved;UCHAR setup[8];};
struct a04_mp_requirements {ULONG common_bytes,max_transfer;};
struct a04_mp_sg {ULONG flags;ULONG_PTR current_va;PVOID mapped_va;ULONG count;};
struct a04_mp_root {ULONG ports;USHORT characteristics,pad;ULONG power_good,current;};
typedef ULONG (*A04_MP_OPEN)(PVOID,struct a04_mp_properties *,PVOID);
typedef VOID (*A04_MP_QUERY)(PVOID,struct a04_mp_properties *,struct a04_mp_requirements *);
typedef VOID (*A04_MP_CLOSE)(PVOID,PVOID,BOOLEAN);
typedef ULONG (*A04_MP_START)(PVOID,struct a04_mp_resources *);
typedef VOID (*A04_MP_STOP)(PVOID,BOOLEAN);
typedef VOID (*A04_MP_VOID)(PVOID);
typedef ULONG (*A04_MP_ONE)(PVOID);
typedef BOOLEAN (*A04_MP_ISR)(PVOID);
typedef VOID (*A04_MP_DPC)(PVOID,BOOLEAN);
typedef ULONG (*A04_MP_SUBMIT)(PVOID,PVOID,struct a04_mp_parameters *,PVOID,struct a04_mp_sg *);
typedef VOID (*A04_MP_ABORT)(PVOID,PVOID,PVOID,PULONG);
typedef ULONG (*A04_MP_EP_GET)(PVOID,PVOID);
typedef VOID (*A04_MP_EP_SET)(PVOID,PVOID,ULONG);
typedef VOID (*A04_MP_EP_POLL)(PVOID,PVOID);
typedef VOID (*A04_MP_ROOT_DATA)(PVOID,struct a04_mp_root *);
typedef ULONG (*A04_MP_ROOT_STATUS)(PVOID,PUSHORT);
typedef ULONG (*A04_MP_PORT_STATUS)(PVOID,USHORT,PULONG);
typedef ULONG (*A04_MP_HUB_STATUS)(PVOID,PULONG);
typedef ULONG (*A04_MP_PORT_FEATURE)(PVOID,USHORT);
typedef VOID (*A04_PORT_INVALIDATE_RH)(PVOID);
typedef VOID (*A04_PORT_INVALIDATE_EP)(PVOID,PVOID);
typedef VOID (*A04_PORT_ASYNC_ROUTINE)(PVOID,PVOID);
typedef NTSTATUS (*A04_PORT_ASYNC)(PVOID,ULONG,PVOID,ULONG,A04_PORT_ASYNC_ROUTINE);
typedef VOID (*A04_PORT_INVALIDATE_CONTROLLER)(PVOID,ULONG);
/* Third argument is the original USBPORT transfer-parameters pointer, not
 * the miniport-private transfer context passed as Submit's fourth argument. */
typedef VOID (*A04_PORT_COMPLETE)(PVOID,PVOID,struct a04_mp_parameters *,USBD_STATUS,ULONG);
struct a04_mp_packet {
 ULONG version,flags,bandwidth,reserved;SIZE_T extension_bytes,endpoint_bytes,transfer_bytes;
 UCHAR name[8];ULONG common_bytes;
 A04_MP_OPEN open,reopen;A04_MP_QUERY query;A04_MP_CLOSE close;
 A04_MP_START start;A04_MP_STOP stop;A04_MP_VOID suspend;A04_MP_ONE resume;
 A04_MP_ISR isr;A04_MP_DPC dpc;A04_MP_SUBMIT submit,submit_iso;A04_MP_ABORT abort;
 A04_MP_EP_GET get_state;A04_MP_EP_SET set_state;A04_MP_EP_POLL poll_endpoint;
 A04_MP_VOID check;A04_MP_ONE frame;A04_MP_VOID next_sof,enable,disable,poll_controller;
 A04_MP_EP_SET toggle;A04_MP_EP_GET get_status;A04_MP_EP_SET set_status;A04_MP_VOID reset;
 A04_MP_ROOT_DATA root_data;A04_MP_ROOT_STATUS root_status;A04_MP_PORT_STATUS port_status;
 A04_MP_HUB_STATUS hub_status;
 A04_MP_PORT_FEATURE port_reset,port_power,port_enable,port_suspend,clear_enable,clear_power,
 clear_suspend,clear_enable_change,clear_connect_change,clear_reset_change,clear_suspend_change,clear_oc_change;
 A04_MP_VOID root_disable,root_enable;
 PVOID send_one,end_one,passthru;
 PVOID port_debug,port_break,port_assert,port_registry;
 A04_PORT_INVALIDATE_RH invalidate_root;A04_PORT_INVALIDATE_EP invalidate_endpoint;
 A04_PORT_COMPLETE complete;PVOID complete_iso,log,mapped_va;A04_PORT_ASYNC async_callback;PVOID config,wait;A04_PORT_INVALIDATE_CONTROLLER invalidate_controller;PVOID bugcheck,notify;
 PVOID rebalance,flush,chirp,take_control;ULONG reserved_tail[2];
};
#if defined(_ARM_)
C_ASSERT(sizeof(struct a04_mp_resources)==0x90);
C_ASSERT(FIELD_OFFSET(struct a04_mp_resources,fdo)==0x7c);
C_ASSERT(FIELD_OFFSET(struct a04_mp_resources,base)==0x20);
C_ASSERT(sizeof(struct a04_mp_properties)==0x40);
C_ASSERT(FIELD_OFFSET(struct a04_mp_properties,type)==0x14);
C_ASSERT(FIELD_OFFSET(struct a04_mp_properties,hub)==0x30);
C_ASSERT(FIELD_OFFSET(struct a04_mp_properties,port)==0x32);
C_ASSERT(FIELD_OFFSET(struct a04_mp_properties,period)==0x6);
C_ASSERT(sizeof(struct a04_mp_parameters)==0x1c);
C_ASSERT(sizeof(struct a04_mp_packet)==0x13c);
C_ASSERT(FIELD_OFFSET(struct a04_mp_packet,start)==0x38);
C_ASSERT(FIELD_OFFSET(struct a04_mp_packet,root_data)==0x90);
C_ASSERT(FIELD_OFFSET(struct a04_mp_packet,complete)==0xfc);
C_ASSERT(FIELD_OFFSET(struct a04_mp_packet,invalidate_root)==0xf4);
C_ASSERT(FIELD_OFFSET(struct a04_mp_packet,async_callback)==0x10c);
C_ASSERT(FIELD_OFFSET(struct a04_mp_packet,invalidate_controller)==0x118);
#endif
#endif
