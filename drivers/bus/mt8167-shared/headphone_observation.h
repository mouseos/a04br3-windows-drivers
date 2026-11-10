/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_HP_OBSERVATION_H
#define A04_HP_OBSERVATION_H
#include <stdint.h>
struct a04_hp_raw_sample {
 uint32_t size,version,generation,channel,valid,phase;int32_t average_uv;
 uint16_t raw[22];
};
struct a04_hp_raw_pair {uint32_t size,version,generation;int32_t result;struct a04_hp_raw_sample samples[2];};
#define IOCTL_MT8167_HP_RAW 0x0022655cu

#endif
