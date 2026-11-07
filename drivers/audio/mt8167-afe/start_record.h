/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_AUDIO_START_RECORD_H
#define MT8167_AUDIO_START_RECORD_H
#ifndef A04_AUDIO_START_PERSIST
#define A04_AUDIO_START_PERSIST 0
#endif
#if A04_AUDIO_START_PERSIST
/* Fixed REG_BINARY, DWORD fields, signed NTSTATUS bits. No kernel pointers.
 * Schema/revision/Complete must match before interpreting a retained record. */
struct Mt8167AudioStartRecord {
 ULONG schema,bytes,inf_revision,stage;NTSTATUS result;
 ULONG system_time_low,system_time_high;
 ULONG dma_called,description_version,scatter_gather,address_width,map_registers;
 ULONG adapter_present,operations_present;
};
enum {A04_START_RESOURCES=1,A04_START_PDO,A04_START_OWNER_POOL,A04_START_DMA,
 A04_START_PARENT_QUERY,A04_START_PARENT_BIND,A04_START_SHARED_ATTACH,
 A04_START_AFE_MAP,A04_START_AFE_BIND,A04_START_IRQ,A04_START_BACKEND_READY,
 A04_START_MINIPORT,A04_START_REGISTER,A04_START_READY};
NTSTATUS Mt8167PersistStartRecord(const Mt8167AudioStartRecord *);
#endif
#endif
