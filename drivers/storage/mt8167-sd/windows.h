/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SD_WINDOWS_H
#define A04_SD_WINDOWS_H
#include <ntddk.h>
#include <wdf.h>
#include <srb.h>
#include <ntddscsi.h>
#include <ntddstor.h>
#include "sd.h"
#include "power_interface.h"
#define SD_QUEUE_LIMIT 8
#define SD_TAG 's61M'
struct sd_host {
 WDFDEVICE device,child;WDFTIMER timer;volatile ULONG *registers;
 EX_RUNDOWN_REF requests;KMUTEX serial;
 IO_CSQ csq;KSPIN_LOCK queue_lock;LIST_ENTRY pending,completions;
 KEVENT consumer_wake,hardware_idle;HANDLE consumer_thread;PIO_WORKITEM completion_item;
 LONG hardware_users;BOOLEAN consumer_online,consumer_stop,completion_scheduled;
 volatile LONG closing,present,generation,attention,ejected,inflight,surprised,blocked,absent_seen,claimed,child_online;
 FAST_MUTEX power_relations_lock;WDFIOTARGET power_target;PDEVICE_OBJECT supplier;struct a04_sd_power_interface power;ULONG power_token;BOOLEAN power_valid;
 BOOLEAN had_d0,quiescing;LONG card_generation,run_generation;
 PIRP active_irp;struct a04_sd_card card;struct a04_sd_ops ops;
};
typedef struct sd_host SD_HOST;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(SD_HOST,SdHost)
struct sd_child {struct sd_host *host;};
typedef struct sd_child SD_CHILD;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(SD_CHILD,SdChild)
struct sd_work {LIST_ENTRY link;PVOID saved_context;volatile LONG refs,finished;struct sd_host *host;WDFDEVICE child;PIRP irp;LONG generation;uint64_t deadline;};
NTSTATUS SdConsumerInitialize(struct sd_host *);
NTSTATUS SdConsumerStart(struct sd_host *);
void SdStopAdmission(struct sd_host *);
NTSTATUS SdConsumerStop(struct sd_host *);
NTSTATUS SdSubmit(WDFDEVICE,PIRP);
void SdExecute(struct sd_work *);
void SdFinish(struct sd_work *,NTSTATUS,ULONG_PTR);
EVT_WDF_OBJECT_CONTEXT_DESTROY SdConsumerDestroy;
NTSTATUS SdPowerConnect(struct sd_host*);
NTSTATUS SdPowerDisconnect(struct sd_host*);
NTSTATUS SdPowerHealth(struct sd_host*);
NTSTATUS SdPowerRelease(struct sd_host*);
EVT_WDFDEVICE_WDM_IRP_PREPROCESS SdPowerPnp;
EVT_WDFDEVICE_WDM_IRP_PREPROCESS SdScsi;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL SdControl;
NTSTATUS SdProcess(struct sd_work *,ULONG_PTR *);
NTSTATUS SdAcpi(struct sd_host *,const char name[4],ULONG *);
NTSTATUS SdCreateChild(struct sd_host *,ULONG *stage);
#endif
