/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SD_WINDOWS_H
#define A04_SD_WINDOWS_H
#include <ntddk.h>
#include <wdf.h>
#include <srb.h>
#include <ntddscsi.h>
#include <ntddstor.h>
#include "sd.h"
#define SD_TAG 's61M'
struct sd_host {
 WDFDEVICE device,child;WDFTIMER timer;volatile ULONG *registers;
 EX_RUNDOWN_REF requests;KMUTEX serial;
 volatile LONG closing,present,generation,attention,ejected,inflight,surprised,blocked,absent_seen,claimed,child_online;
 BOOLEAN had_d0,quiescing;LONG card_generation,run_generation;
 PIRP active_irp;struct a04_sd_card card;struct a04_sd_ops ops;
};
typedef struct sd_host SD_HOST;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(SD_HOST,SdHost)
struct sd_child {struct sd_host *host;};
typedef struct sd_child SD_CHILD;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(SD_CHILD,SdChild)
struct sd_work {PIO_WORKITEM item;struct sd_host *host;WDFDEVICE child;PIRP irp;LONG generation;uint64_t deadline;};
EVT_WDFDEVICE_WDM_IRP_PREPROCESS SdScsi;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL SdControl;
NTSTATUS SdProcess(struct sd_work *,ULONG_PTR *);
NTSTATUS SdAcpi(struct sd_host *,const char name[4],ULONG *);
NTSTATUS SdCreateChild(struct sd_host *);
#endif
