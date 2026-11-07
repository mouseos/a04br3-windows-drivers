/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_INIT_TRACE_H
#define A04_WINDOWS_INIT_TRACE_H
#include <ntddk.h>
#include "checkpoint_wire.h"
/* Optional, synchronous PASSIVE observation only. Never retains a callback. */
typedef void (*a04_session_trace_fn)(void *,unsigned,unsigned,unsigned,ULONG,ULONG);
#define A04_INIT_RESULT(stage,kind,raw,detail) do { \
 if(observe&&observation)observe(observation,stage,A04_CP_RESULT,kind,(ULONG)(raw),(ULONG)(detail)); \
} while(0)
#endif
