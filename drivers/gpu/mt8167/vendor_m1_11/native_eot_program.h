/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_EOT_PROGRAM_H
#define A04_M111_NATIVE_EOT_PROGRAM_H
#include <stdint.h>
struct a04_m111_eot_program {
    uint8_t usc[16],pds[40];
    uint32_t usc_bytes,pds_data_words,pds_code_words,temporary_count,shared_count;
};
/* Single PBE emit. The two final emit words are supplied by an independent
 * PBE-state builder; they are not the 3D record's two 64-bit CSR registers.
 * USC code: source-compiled GE8300 22.40.54.30 emitpix(sh0,sh1).freep.end.
 * PDS: vendor's one-DMA descriptor, DOUTW sh0/sh1 then DOUTU.
 * usc_gpu must be 4-byte aligned in [0xba00000000,0xba40000000).
 * Upload PDS at a 16-byte-aligned VA: data at +0, code at +32 (40 bytes total).
 * Ordinary CPU shadows only. This builds neither a render payload nor its
 * pixel-event register binding, and does not authorize queue publication. */
int a04_m111_build_eot_program(struct a04_m111_eot_program *out,
    uint64_t usc_gpu,uint32_t emit_word0,uint32_t emit_word1);
#endif
