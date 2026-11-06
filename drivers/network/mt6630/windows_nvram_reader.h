/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_NVRAM_READER_H
#define A04_WINDOWS_NVRAM_READER_H
#include <ntddk.h>
/* PASSIVE_LEVEL, read-only system-disk GPT discovery. No fallback defaults,
 * caller files or disk numbers. Output is erased on failure. */
NTSTATUS a04_windows_nvram_read(UCHAR result[512]);
NTSTATUS a04_windows_bt_nvram_read(UCHAR result[36]);
#endif
