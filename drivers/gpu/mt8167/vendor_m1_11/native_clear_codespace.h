/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_CLEAR_CODESPACE_H
#define A04_M111_NATIVE_CLEAR_CODESPACE_H
#include <stdint.h>
#define A04_M111_CLEAR_USC_BYTES 40u
#define A04_M111_CLEAR_PDS_BYTES 0xe4u
#define A04_M111_CLEAR_BG_PIXEL_DATA 0u
#define A04_M111_CLEAR_BG_PIXEL_CODE 0x20u
#define A04_M111_CLEAR_BG_SHARED_DATA 0x40u
#define A04_M111_CLEAR_BG_SHARED_CODE 0x50u
#define A04_M111_CLEAR_EOT_DATA 0x80u
#define A04_M111_CLEAR_EOT_CODE 0xa0u
#define A04_M111_CLEAR_TA_PPP_DATA 0xc0u
#define A04_M111_CLEAR_TA_PPP_CODE 0xe0u
#define A04_M111_CLEAR_TA_PPP_USC 0x20u
struct a04_m111_clear_codespace {
    uint8_t usc[A04_M111_CLEAR_USC_BYTES];
    uint8_t pds[A04_M111_CLEAR_PDS_BYTES];
};
/* CPU shadows, no allocation or publication. Both complete spans must fit the
 * respective 1GiB execution heap. Owner must map/copy before GPU ownership.
 * packed_colour is the output-register-0 bit pattern, not an API RGBA value.
 * emit words are final PBE emit parameters, not record CSR qwords.
 * No complete payload implied. VDM/PPP state is a separate resource. */
int a04_m111_build_clear_codespace(struct a04_m111_clear_codespace *out,
    uint64_t usc_gpu,uint64_t pds_gpu,uint32_t packed_colour,
    uint32_t emit_word0,uint32_t emit_word1);
/* Binding for the exact programs above. Addresses are PDS heap-relative.
 * Caller selects PR/background record slot; this does not patch a record. */
struct a04_m111_clear_code_bindings {
    uint64_t background[3];
    uint32_t pixel_event_info,pixel_event_code,pixel_event_data;
    uint32_t background_pixel_data_bytes;
    uint32_t ta_ppp_pds_base; /* combined data/code base for TA record +0x40 */
};
int a04_m111_build_clear_code_bindings(struct a04_m111_clear_code_bindings *out,
    uint64_t pds_gpu);
#endif
