/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_CLEAR_CODESPACE_H
#define A04_M111_NATIVE_CLEAR_CODESPACE_H
#include <stdint.h>
#define A04_M111_CLEAR_USC_BYTES 32u
#define A04_M111_CLEAR_PDS_BYTES 0xa8u
#define A04_M111_CLEAR_BG_PIXEL_DATA 0u
#define A04_M111_CLEAR_BG_PIXEL_CODE 0x20u
#define A04_M111_CLEAR_BG_SHARED_DATA 0x40u
#define A04_M111_CLEAR_BG_SHARED_CODE 0x50u
#define A04_M111_CLEAR_EOT_DATA 0x80u
#define A04_M111_CLEAR_EOT_CODE 0xa0u
struct a04_m111_clear_codespace {
    uint8_t usc[A04_M111_CLEAR_USC_BYTES];
    uint8_t pds[A04_M111_CLEAR_PDS_BYTES];
};
/* CPU shadows, no allocation or publication. Both complete spans must fit the
 * respective 1GiB execution heap. Owner must map/copy before GPU ownership.
 * packed_colour is the output-register-0 bit pattern, not an API RGBA value.
 * emit words are final PBE emit parameters, not record CSR qwords.
 * No background/event register binding, VDM/PPP or complete payload implied. */
int a04_m111_build_clear_codespace(struct a04_m111_clear_codespace *out,
    uint64_t usc_gpu,uint64_t pds_gpu,uint32_t packed_colour,
    uint32_t emit_word0,uint32_t emit_word1);
#endif
