/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WIFI_CHECKPOINT_H
#define A04_WIFI_CHECKPOINT_H
#include <ntddk.h>
#include "checkpoint_wire.h"
struct a04_checkpoint {HANDLE key;struct a04_cp_record *record;ULONG channel;};
void a04_cp_initialize(void);
NTSTATUS a04_cp_begin(struct a04_checkpoint *,ULONG,uint64_t);
NTSTATUS a04_cp_emit(struct a04_checkpoint *,ULONG,ULONG,ULONG,ULONG,ULONG);
NTSTATUS a04_cp_finish(struct a04_checkpoint *,ULONG,ULONG,ULONG);
void a04_cp_trace(void *,unsigned,unsigned,unsigned,ULONG,ULONG);
#endif
