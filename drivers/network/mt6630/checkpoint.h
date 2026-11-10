/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WIFI_CHECKPOINT_H
#define A04_WIFI_CHECKPOINT_H
#include <ntddk.h>
#include "checkpoint_wire.h"
struct a04_checkpoint {HANDLE key;struct a04_cp_record *record;ULONG channel,rolling;};
/* Serialized PASSIVE worker only; cleanup follows worker drain. Never call
 * while an NDIS spin lock is held or from timer/DPC/send entry points. */
struct a04_connection_checkpoint {struct a04_checkpoint cp;ULONG samples[32];};
void a04_cp_connection_begin(struct a04_connection_checkpoint *,uint64_t,ULONG);
void a04_cp_connection_emit(struct a04_connection_checkpoint *,ULONG,ULONG,ULONG,ULONG,ULONG);
void a04_cp_connection_end(struct a04_connection_checkpoint *,ULONG,ULONG);
void a04_cp_initialize(void);
NTSTATUS a04_cp_begin(struct a04_checkpoint *,ULONG,uint64_t);
NTSTATUS a04_cp_emit(struct a04_checkpoint *,ULONG,ULONG,ULONG,ULONG,ULONG);
NTSTATUS a04_cp_finish(struct a04_checkpoint *,ULONG,ULONG,ULONG);
void a04_cp_trace(void *,unsigned,unsigned,unsigned,ULONG,ULONG);
#endif
