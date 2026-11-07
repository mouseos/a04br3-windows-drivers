/* SPDX-License-Identifier: MIT */
#ifndef A04_WINDOWS_NATIVE_JOB_PRIVATE_H
#define A04_WINDOWS_NATIVE_JOB_PRIVATE_H
#include "windows_native_cold_private.h"
#include "native_clear_job.h"
#include "native_pair.h"
struct a04_vendor_native_candidate {
    BOOLEAN attempted,verified;
    struct a04_m111_clear_job job;
    struct a04_m111_pair_result pair;
    uint8_t rings[8192];
};
struct a04_vendor_native_result {uint32_t phase,matching_pixels,mismatched_pixels,guard_errors;};
struct a04_vendor_start;
/* Dormant validation implementation: NO provider/IOCTL/service caller exists.
 * A future activation must hold the provider lifetime lock and review gate. */
NTSTATUS a04_vendor_native_publish_candidate(struct a04_vendor_native_cold *,
    struct a04_vendor_start *,struct a04_vendor_native_result *);
/* Private cold candidate only. No KCCB/UFO/cleanup counter publication.
 * An eventual publisher must revalidate live queue offsets and retain the
 * existing command_busy / lifetime lock / poison protocol. */
#endif
