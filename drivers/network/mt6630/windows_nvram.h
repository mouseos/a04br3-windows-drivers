/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_NVRAM_H
#define A04_WINDOWS_NVRAM_H
#include <ntddk.h>
#include <ndis.h>
/* Asynchronous job. One owner, one start, and drain before freeing; callback
 * must not drain its own job. Result exists only during the callback. */
typedef void (*a04_nvram_completion)(void *,NTSTATUS,const UCHAR *);
struct a04_windows_nvram_job {
 NDIS_HANDLE work; KEVENT done; volatile LONG started;
 a04_nvram_completion complete; void *context;
};
NDIS_STATUS a04_windows_nvram_init(struct a04_windows_nvram_job *,NDIS_HANDLE,
 a04_nvram_completion,void *);
NDIS_STATUS a04_windows_nvram_start(struct a04_windows_nvram_job *);
void a04_windows_nvram_drain(struct a04_windows_nvram_job *);
#endif
