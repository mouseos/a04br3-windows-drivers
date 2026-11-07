/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_CAPTURE_INPUT_OWNER_H
#define A04_CAPTURE_INPUT_OWNER_H
#include "codec_capture.h"
/* Parent resource lock held, PASSIVE_LEVEL. These callbacks must use the
 * parent's existing VAUD/audio-clock mappings, not acquire a second MMIO map.
 * reserve excludes playback/HFP and every other AFE owner before writes.
 * A failed mutating callback keeps the reservation and all earlier supplies.
 */
struct a04_capture_input_owner {
 void *context;
 int (*reserve)(void *);
 int (*unreserve)(void *);
 int (*power_acquire)(void *,unsigned);
 int (*power_release)(void *);
 /* Independent controller DMA + IRQ lifetime proof, NOT AFE enable readback. */
 int (*verify_drained)(void *);
 struct a04_capture_codec_io io;
 struct a04_capture_codec codec;
 unsigned online,reserved,power_held,ready,poisoned;
};
#ifdef __cplusplus
extern "C" {
#endif
int a04_capture_input_acquire(struct a04_capture_input_owner *,unsigned);
int a04_capture_input_release(struct a04_capture_input_owner *);
void a04_capture_input_offline(struct a04_capture_input_owner *);
#ifdef __cplusplus
}
#endif
#endif
