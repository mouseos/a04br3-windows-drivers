/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_HI556_WINDOWS_CAPTURE_H
#define A04_HI556_WINDOWS_CAPTURE_H
#include <ntddk.h>
#include <wdf.h>
#include <acpiioct.h>
#include "windows_owner_client.h"
#include "capture.h"
#define A04_CAM_ALLOC_BYTES (A04_HI556_FRAME_BYTES+8192u)
#define A04_CAM_MAGIC 0x4134434du
#include "capture_wire.h"
static const GUID A04Hi556CaptureGuid=A04_HI556_CAPTURE_GUID_VALUES;
_Static_assert(sizeof(struct a04_cam_identity_wire)==sizeof(A04_HI556_IDENTITY_RESULT),"identity snapshot ABI");
typedef struct {
 WDFDEVICE device;
 UCHAR *region[4];ULONG region_size[4];
 A04_HI556_POWER_INTERFACE power;
 A04_I2C1_IDENTITY_INTERFACE identity;
 A04_I2C1_CAMERA_INTERFACE sensor;
 PDMA_ADAPTER adapter;PVOID common;PHYSICAL_ADDRESS logical;
 volatile LONG online,leases;ULONG generation;
 BOOLEAN quarantined;
 A04_CAM_STATUS status;
} A04_CAM_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(A04_CAM_CONTEXT,CameraContext)
NTSTATUS a04_cam_qualified(A04_CAM_CONTEXT *);
NTSTATUS a04_cam_identify(A04_CAM_CONTEXT *);
void a04_cam_power_init(A04_CAM_CONTEXT *);
NTSTATUS a04_cam_frame(A04_CAM_CONTEXT *,WDFREQUEST,void *,size_t,ULONG_PTR *);
void a04_cam_free_dma(A04_CAM_CONTEXT *);
#endif
