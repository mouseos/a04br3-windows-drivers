/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_SD_H
#define A04_MT8167_SD_H
#include <stdint.h>
#include <stddef.h>
#include "../mt8167-msdc/pio.h"
#define A04_SD_BASE 0x11130000u
#define A04_SD_IRQ 111u /* DTS GIC_SPI 79 + architectural SPI base32 */
#define A04_SD_MAX_BLOCKS 128u
/* Single CPU/PIO owner. Never enables DMA, CMD1, eMMC cache/partition commands,
 * UHS/1.8V, tuning, CMD18/25 or automatic write retries. */
enum a04_sd_result {SD_OK=0,SD_INVALID,SD_REMOVED,SD_CANCELLED,SD_TIMEOUT,SD_CRC,SD_CARD,SD_UNSUPPORTED,SD_PROTECTED,SD_POISONED};
struct a04_sd_ops {
 struct a04_msdc_io io;
 uint64_t (*now_100ns)(void *);void (*delay_us)(void *,unsigned);
 int (*live)(void *); /* 1=current D0/media/request generation, 0=revoked */
};
struct a04_sd_card {
 uint64_t sectors;uint32_t cid[4],csd[4],ocr;uint16_t rca;
 uint8_t high_capacity,write_protected,ready,poisoned,mid,product[5],revision;
};
/* deadline is one absolute monotonic 100ns deadline covering the whole request. */
enum a04_sd_result a04_sd_initialize(const struct a04_sd_ops *,struct a04_sd_card *,uint64_t deadline);
enum a04_sd_result a04_sd_transfer(const struct a04_sd_ops *,struct a04_sd_card *,uint64_t lba,uint32_t blocks,int write,uint8_t *,uint64_t deadline);
enum a04_sd_result a04_sd_barrier(const struct a04_sd_ops *,struct a04_sd_card *,uint64_t deadline);
/* D0 owner only; after CPU rundown. Bounds reset, masks host events, no DMA. */
enum a04_sd_result a04_sd_quiesce(const struct a04_sd_ops *,uint64_t deadline);
#endif
