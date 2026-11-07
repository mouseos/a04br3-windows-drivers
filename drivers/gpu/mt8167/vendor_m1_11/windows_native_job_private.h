/* SPDX-License-Identifier: MIT */
#ifndef A04_WINDOWS_NATIVE_JOB_PRIVATE_H
#define A04_WINDOWS_NATIVE_JOB_PRIVATE_H
#include "windows_native_cold_private.h"
#include "native_clear_job.h"
#include "native_pair.h"
#ifndef A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
#define A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE 0
#endif
#if A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
#include "native_triangle_suite.h"
#endif
struct a04_vendor_native_candidate {
    BOOLEAN attempted,verified,published;
    uint32_t completed_frames,start[2];
    struct a04_m111_clear_job job;
#if A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE
    struct a04_m111_triangle_suite triangle;
#endif
    struct a04_m111_pair_result pair;
    uint8_t rings[8192];
};
struct a04_vendor_start;
/* Private publisher used only by the opaque activation wrapper.
 * D0 activation is separately default-OFF; no arbitrary-packet IOCTL exists. */
NTSTATUS a04_vendor_native_publish_candidate(struct a04_vendor_native_cold *,
    struct a04_vendor_start *,struct a04_vendor_native_result *);
/* Private cold candidate only. No KCCB/UFO/cleanup counter publication.
 * An eventual publisher must revalidate live queue offsets and retain the
 * existing command_busy / lifetime lock / poison protocol. */
#endif
