/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_RECORD_FIELDS_H
#define A04_M111_NATIVE_RECORD_FIELDS_H
#include <stddef.h>
#include <stdint.h>
#define A04_M111_TA_BYTES 0x88u
#define A04_M111_3D_BYTES 0x150u
/* Proven services-owned fields only, not a complete GPU task. Word names
 * intentionally preserve unknown register semantics. No zero template or
 * submit API is provided. Apply to an independently initialized record. */
struct a04_m111_ta_fields {
    uint64_t word08, word30, word78;
    uint32_t flags, frame, hwrt, zbuffer, sbuffer, msaa;
    uint32_t pr_fence_addr, pr_fence_value, overlap_addr, overlap_value;
};
struct a04_m111_3d_fields {
    uint64_t dummy_stencil;
    uint32_t word00, word1c, word20, flags, frame, hwrt, zbuffer, sbuffer, msaa;
    uint32_t zls_stride,sls_stride;
};
/* GLES 0x15e9c8 / 0x1612a0: remaining proven TA register stores.
 * word20 is intentionally unnamed; register semantics remain separate from
 * the observed address/value layout. No writes to 0x3c/0x44/0x80 holes. */
struct a04_m111_ta_registers {
    uint64_t ppp_ctrl,vdm_stream,border_table,word20,tpu;
    uint32_t word38,pds_code40;
};
int a04_m111_patch_ta_registers(void *record,size_t bytes,
    const struct a04_m111_ta_registers *registers);
uint32_t a04_m111_ta_flags(uint32_t client_flags,uint32_t rt_flags);
uint32_t a04_m111_3d_flags(uint32_t client_flags,uint32_t rt_flags,int context_flag);
/* Writes only the offsets proved by libsrv_um RGXKickTA / FUN_0012877c.
 * Untouched bytes are neither validated nor declared to be padding. */
int a04_m111_patch_ta_fields(void *record,size_t bytes,const struct a04_m111_ta_fields *fields);
int a04_m111_patch_3d_fields(void *record,size_t bytes,const struct a04_m111_3d_fields *fields);
/* libGLESv2_mtk FUN_001c2174 -> FUN_0020ff6c: eight pairs of
 * 64-bit PBE CSR registers, not modern FWIF's eight triples. EOT emit words
 * live separately from this record and must be generated independently. */
int a04_m111_patch_3d_pbe(void *record,size_t bytes,const uint64_t csr[8][2]);
/* FUN_0015e9c8 calls FUN_0011372c with surface+0x6b8/+0x6d0.
 * Preserve offset names until PR/background roles are fully reconciled. */
int a04_m111_patch_3d_background(void *record,size_t bytes,
    const uint64_t words_f8[3],const uint64_t words_110[3]);
#endif
