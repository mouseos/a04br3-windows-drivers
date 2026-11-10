/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_CLOCK_SNAPSHOT_H
#define MT8167_CLOCK_SNAPSHOT_H
#include <stdint.h>
#define MT8167_CLOCK_SNAPSHOT_VERSION 1u
#define MT8167_CLOCK_REGISTER_COUNT 11u
#define MT8167_CLOCK_UNSTABLE 1u
#define MT8167_CLOCK_READ_FAILED 2u
#define MT8167_CLOCK_TIME_REVERSED 4u
#define MT8167_CLOCK_REFERENCE_UNKNOWN 8u
#define MT8167_CLOCK_DDR_UNAVAILABLE 16u
#define MT8167_CLOCK_EFFECTIVE_CPU_UNKNOWN 32u
#define MT8167_CLOCK_ADMISSION_LOST 64u
#define MT8167_CLOCK_INFRA 0u
#define MT8167_CLOCK_APMIXED 1u
/* Fixed 128-byte data ABI: no addresses, caller pointers, or inferred Hz.
 * valid masks identify actual samples; zero-filled unread slots are not data.
 * Equal sweeps do not rule out firmware changes between samples (ABA). */
struct mt8167_clock_snapshot {
    uint32_t version, bytes, flags, valid_first, valid_second, register_count;
    uint64_t before_100ns, after_100ns;
    uint32_t first[MT8167_CLOCK_REGISTER_COUNT];
    uint32_t second[MT8167_CLOCK_REGISTER_COUNT];
};
struct mt8167_clock_snapshot_io {
    void *context;
    int (*read_index)(void *, unsigned, uint32_t *);
    uint64_t (*now_100ns)(void *);
};
/* Index-only whitelist; never accept an arbitrary MMIO address from a caller. */
int mt8167_clock_register(unsigned index, unsigned *bank, unsigned *offset);
/* Parent supplies existing mapping, lock, admission and rundown protection.
 * On read/time failure return -1 with explicit partial masks, never a rate.
 * Both complete unequal sweeps return 0 with UNSTABLE set. */
int mt8167_clock_snapshot_collect(const struct mt8167_clock_snapshot_io *,
                                 struct mt8167_clock_snapshot *);
#endif
