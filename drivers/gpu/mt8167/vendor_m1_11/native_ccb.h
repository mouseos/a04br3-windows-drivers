/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_CCB_H
#define A04_M111_NATIVE_CCB_H
#include "A04BR3RogueDDKABI.h"
enum a04_m111_client_type {
    A04_M111_CLIENT_TA=0x2abc80c9u,
    A04_M111_CLIENT_3D=0x2abc80cau,
    A04_M111_CLIENT_PR=0x2abc80ceu,
    A04_M111_CLIENT_FENCE=0x2abc00d5u,
    A04_M111_CLIENT_UPDATE=0x2abc00d6u,
    A04_M111_CLIENT_FENCE_PR=0x2abc00d8u
};
struct a04_m111_ccb_result { uint32_t write_offset,header_offset,padding_bytes; };
/* Verified KM envelope only. payload is copied, not interpreted or certified.
 * In particular, using native_record_fields does NOT initialize a complete
 * TA/3D payload. No MMIO, firmware publication, or runtime submission here.
 * ring is 4096 bytes of ordinary CPU shadow memory. Failure changes nothing. */
int a04_m111_ccb_append(void *ring,size_t ring_bytes,uint32_t read_offset,
    uint32_t write_offset,enum a04_m111_client_type type,const void *payload,
    size_t payload_bytes,uint32_t external_job,uint32_t internal_job,
    struct a04_m111_ccb_result *result);
/* No optional Z/S/MSAA resources. HWRT cleanup is attached on the requested
 * data master. Caller owns cleanup lifetime and all queue publication. */
int a04_m111_build_render_kick(RGXFWIF_KCCB_CMD *out,uint32_t context_fw,
    unsigned dm,uint32_t client_write,uint32_t hwrt_fw);
#endif
