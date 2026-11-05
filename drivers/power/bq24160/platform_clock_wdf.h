/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ_PLATFORM_CLOCK_WDF_H
#define A04_BQ_PLATFORM_CLOCK_WDF_H
#include <ntddk.h>
#include <wdf.h>
#include "platform_clock.h"
struct a04_bq_clock_observation { uint64_t qpc_before,qpc_after,frequency,firmware_last,firmware_exit,firmware_frequency,record_state; };
enum a04_bq_clock_result a04_bq_clock_observe(WDFDEVICE,struct a04_bq_clock_observation *,NTSTATUS *);
#endif
