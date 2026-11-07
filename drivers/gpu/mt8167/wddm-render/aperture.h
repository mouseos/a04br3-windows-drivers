/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_APERTURE_H
#define GEO_APERTURE_H
#include <stdint.h>
/* Exact exclusive GPU VA window reserved by the native page-table owner.
 * No guessed CPU BAR, no physical identity assumption, no cache-coherence bit. */
struct geo_aperture_config {
 uint32_t size,version;
 uint64_t gpu_base,bytes,commit_limit,highest_dma_address;
};
#endif
