/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_COPY_JOB_H
#define A04_NATIVE_COPY_JOB_H
#include <stdint.h>
/* Internal CPU producer, NOT a user wire ABI or DMA/ownership proof. The
 * consumer must own both real mapped allocation spans through the complete
 * TQ/UFO/SLC/GP/CPU-ownership transition. No CPU source Lock is required. */
struct a04_native_copy_job_input {
 uint64_t aperture_base,aperture_bytes,source_va,destination_va;
 uint64_t source_bytes,destination_bytes,shared_va;
 uint32_t width,height,pitch_pixels;
 uint32_t fragment_pds_offset,secondary_data_offset,secondary_code_offset;
 uint32_t eot_pds_offset,fragment_usc_offset;
};
struct a04_native_copy_job_image {
 uint32_t task[52],shared[9],fragment_pds[16];
 uint32_t secondary_data[4],secondary_code[4],eot_pds[16];
};
/* Complete direct native22 TQ record for disjoint equally-pitched BGRA8
 * resources, one mip/sample, integer origin/extent. No guessed register bits:
 * reuses the exact native_copy_state / clear_image DOD producers. The fixed
 * copy USC asset must separately pass native_copy_usc_match before staging.
 * No GPU command is submitted, resource mapped, or completion manufactured. */
int a04_native_copy_job_build(struct a04_native_copy_job_image *,
 const struct a04_native_copy_job_input *);
struct a04_native_clear_job_input {
 uint64_t aperture_base,aperture_bytes,target_va,target_bytes;
 uint32_t width,height,pitch_pixels,pds_offset,colour;
};
/* Native22 fast fill uses its CCB colour word, not a rewritten USC/PDS
 * program. The consumer stages the returned EOT PDS only while cold. */
int a04_native_clear_job_build(struct a04_native_copy_job_image *,
 const struct a04_native_clear_job_input *);
#endif
