/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SD_CHECKPOINT_H
#define A04_SD_CHECKPOINT_H
#include <ntddk.h>
/* Fixed scalar-only record in this service's own NativeCheckpoint key.
 * Registry write acceptance is not a durable-disk or hardware success claim. */
void SdCheckpointInitialize(PUNICODE_STRING);
void SdCheckpoint(PCWSTR,ULONG,NTSTATUS,BOOLEAN);
void SdCheckpointClose(void);
#endif
