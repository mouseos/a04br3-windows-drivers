/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_NVRAM_READER_H
#define A04_WINDOWS_NVRAM_READER_H
#include <ntddk.h>
#include "windows_init_trace.h"
/* PASSIVE_LEVEL, read-only system-disk GPT discovery. No fallback defaults,
 * caller files or disk numbers. Output is erased on failure. */
NTSTATUS a04_windows_nvram_read(UCHAR result[512]);
NTSTATUS a04_windows_nvram_read_traced(UCHAR result[512],a04_session_trace_fn,void *);
#endif
