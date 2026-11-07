/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_CODEC_CAPTURE_H
#define A04_MT8167_CODEC_CAPTURE_H
#include <stdint.h>
/* Parent-owned APMIXED and AFE IO, not a new mapping/PMIC broker. Requires
 * actual VAUD rails, audio clocks and exclusive codec/AFE ownership held by
 * parent before entry. Stereo CH0 uses AU_VIN0/AU_VIN2 in board machine DAPM.
 * Does not establish physical mic presence or a measured codec/FIFO latency. */
struct a04_capture_codec_io {
 void *context;
 int (*analog_read)(void *,unsigned,uint32_t *);
 int (*analog_write)(void *,unsigned,uint32_t);
 int (*afe_read)(void *,unsigned,uint32_t *);
 int (*afe_write)(void *,unsigned,uint32_t);
};
struct a04_capture_codec {uint32_t saved[6];unsigned retained,prepared,poisoned;};
#ifdef __cplusplus
extern "C" {
#endif
/* -1 read-only rejection, -2 uncertain/partial hardware change, 0 prepared.
 * Once retained, parent cannot drop power merely because this call failed. */
int a04_capture_codec_prepare(struct a04_capture_codec *,const struct a04_capture_codec_io *);
/* Only before memif publication, or while a separate verified bus-drain
 * provider still retains all mappings. Register stop is not a drain proof. */
int a04_capture_codec_stop(struct a04_capture_codec *,const struct a04_capture_codec_io *);
#ifdef __cplusplus
}
#endif
#endif
