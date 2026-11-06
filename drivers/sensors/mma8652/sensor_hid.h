/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MMA_SENSOR_HID_H
#define A04_MMA_SENSOR_HID_H
#include <stddef.h>
#include <stdint.h>
#include "mma8652.h"
#define A04_MMA_HID_INPUT_SIZE 9u
#define A04_MMA_HID_FEATURE_SIZE 14u
#define A04_MMA_HID_STATE_READY 2u
#define A04_MMA_HID_EVENT_DATA_UPDATED 4u
struct a04_mma_hid_features { uint8_t reporting; uint16_t sensitivity; uint32_t interval_ms; };
int a04_mma_hid_changed(const struct a04_mma_hid_features *, const struct a04_mma8652_sample *, const struct a04_mma8652_sample *, int);
const uint8_t *a04_mma_hid_descriptor(size_t *length);
void a04_mma_hid_input(const struct a04_mma8652_sample *, uint8_t out[A04_MMA_HID_INPUT_SIZE]);
void a04_mma_hid_get_feature(const struct a04_mma_hid_features *, uint8_t out[A04_MMA_HID_FEATURE_SIZE]);
int a04_mma_hid_set_feature(struct a04_mma_hid_features *, const uint8_t *, size_t);
#endif
