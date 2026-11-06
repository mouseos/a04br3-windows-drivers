/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_AUDIO_DIAGNOSTIC_H
#define A04_AUDIO_DIAGNOSTIC_H
#include <stdint.h>
/* Read-only, fixed ARM32/user ABI. No pointers, register-write requests or
 * implied DMA drain. Values are sequential observations, not an atomic trace. */
#define A04_AUDIO_READ_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN,0x847,METHOD_BUFFERED,FILE_READ_ACCESS)
#define A04_AUDIO_DIAG_VERSION 1u
#define A04_CODEC_VALID_PARENT 1u
#define A04_CODEC_VALID_CLOCK 2u
#define A04_CODEC_VALID_ANALOG 4u
#define A04_CODEC_VALID_SPEAKER 8u
#define A04_CODEC_VALID_RAILS 16u
#define A04_CODEC_VALID_TRIM_OC 32u
#define A04_CODEC_VALID_AFE 64u
#define A04_AUDIO_VALID_PARENT 1u
#define A04_AUDIO_VALID_AFE 2u
#define A04_AUDIO_VALID_POSITION 4u
#define A04_AUDIO_VALID_DMA 8u
struct a04_codec_snapshot {
 uint32_t valid,online,leased,poisoned,power_pinned,trim_captured,trim_code;
 uint32_t digital_prepared,voice_active,top_clock;
 uint32_t analog[5]; /* APMIXED 0x700..0x710 */
 uint32_t spk0,spk9,spk11,trim_raw,overcurrent,pmic_clock;
 uint32_t vaud22,vaud22_selector,vaud28;
 uint32_t afe_dac0,afe_top,afe_cur,afe_irq;
};
struct a04_audio_snapshot {
 uint32_t size,version,sequence,valid,stage,last_status,failure_stage,online,transport;
 uint32_t irq_events,irq_faults,irq_active,shared_quarantined,digital_retained;
 uint32_t dma_base,dma_end,dma_bytes,dma_exposed;
 uint32_t dac0,afe_top,dl1_cur,irq_status,position_bytes;
 struct a04_codec_snapshot codec;
};
enum a04_audio_stage { A04_STAGE_CREATED=1,A04_STAGE_DIGITAL=2,A04_STAGE_VOICE=3,
 A04_STAGE_DMA=4,A04_STAGE_UNMUTE=5,A04_STAGE_RUNNING=6,A04_STAGE_STOP=7,
 A04_STAGE_STOPPED_PINNED=8,A04_STAGE_REVOKED=9 };
#endif
