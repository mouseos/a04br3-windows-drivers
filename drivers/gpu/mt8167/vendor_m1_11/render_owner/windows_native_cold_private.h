/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_M111_WINDOWS_NATIVE_COLD_PRIVATE_H
#define MT8167_M111_WINDOWS_NATIVE_COLD_PRIVATE_H
#include "windows_native_cold.h"
#include "windows_render_cold.h"
#include "native_resources.h"
#include "native_clear_surface.h"
#include "native_clear_codespace.h"
#ifndef A04_GE8300_VENDOR_NATIVE_CLEAR_JOB
#define A04_GE8300_VENDOR_NATIVE_CLEAR_JOB 0
#endif
#ifndef A04_GE8300_NATIVE_OWNER_MDL_TARGET
#define A04_GE8300_NATIVE_OWNER_MDL_TARGET 0
#endif
#ifndef A04_GE8300_NATIVE_OWNER_DRAW
#define A04_GE8300_NATIVE_OWNER_DRAW 0
#endif
struct a04_native_mdl_binding;
struct a04_vendor_native_candidate;
#define A04_NATIVE_OUTPUT_VA UINT64_C(0x8200000000)
#define A04_NATIVE_OUTPUT_BYTES 4096000u
#define A04_NATIVE_USC_VA UINT64_C(0xba00010000)
#define A04_NATIVE_PDS_VA UINT64_C(0xa800010000)
struct a04_vendor_native_cold {
    struct a04_vendor_render_cold *render;
    struct a04_vendor_uncached data,pt,code[2]; /* data is the FIRST real block only */
    struct a04_vendor_uncached data_tail[31],pixel_tail[63];
    ULONG data_blocks,pixel_blocks;
    struct a04_vendor_native_diagnostic *diagnostic; /* only during cold preparation */
    struct a04_vendor_uncached output[4]; /* first pixel block, PD, PT0, PT1 */
    struct a04_m111_clear_surface clear_surface;
    struct a04_m111_clear_code_bindings bindings;
    struct a04_vendor_native_candidate *candidate;
    struct a04_native_mdl_binding *registered_target,*registered_vertex;
    uint64_t vertex_offset;
#if A04_GE8300_NATIVE_OWNER_MDL_TARGET
    uint64_t target_offset;
#endif
    ULONG suffix_offset;
    BOOLEAN output_mapped,backing_staged,quarantined;
    ULONG code_mapped; /* owned USC/PDS PTEs; retained after firmware exposure */
    struct a04_m111_target_storage target;
    struct a04_m111_mlist_storage mlists;
    struct a04_m111_auxiliary_layout auxiliary;
    RGXFWIF_FREELIST freelist[2];
    struct a04_m111_native_addresses addresses;
    uint64_t pm_pages[296]; /* local 40 + global 256, adapter logical addresses */
    ULONG width,height,pm_offset[2],list_offset[2],used;
    BOOLEAN mapped,prepared,bank_prepared,bank_touched;
    struct {struct a04_native_mdl_binding *binding;uint64_t offset;} linear[8];
    ULONG linear_count;
};

ULONG a04_vendor_native_output_bytes(const struct a04_vendor_native_cold *);
BOOLEAN a04_vendor_native_output_ready(const struct a04_vendor_native_cold *);
#define A04_NATIVE_BLOCK_BYTES 65536u
struct a04_vendor_uncached *a04_vendor_native_block(struct a04_vendor_native_cold *,int,ULONG);
NTSTATUS a04_vendor_native_storage_allocate(struct a04_vendor_native_cold *,int,PDMA_ADAPTER,ULONG);
void a04_vendor_native_storage_free(struct a04_vendor_native_cold *,int);
void *a04_vendor_native_storage_pointer(struct a04_vendor_native_cold *,int,ULONG,ULONG);
uint64_t a04_vendor_native_storage_dma(struct a04_vendor_native_cold *,int,ULONG);
int a04_vendor_native_storage_write(struct a04_vendor_native_cold *,int,ULONG,const void *,ULONG);
int a04_vendor_native_storage_read(struct a04_vendor_native_cold *,int,ULONG,void *,ULONG);
void a04_vendor_native_diagnose(struct a04_vendor_native_cold *,ULONG,ULONG,ULONG);
NTSTATUS a04_vendor_native_prepare_job_cold(struct a04_vendor_native_cold *);
#if A04_GE8300_NATIVE_OWNER_MDL_TARGET
NTSTATUS a04_vendor_native_prepare_job_cold_colour(struct a04_vendor_native_cold *,uint32_t);
#endif
uint64_t a04_vendor_native_output_gpu(const struct a04_vendor_native_cold *);
void a04_vendor_native_free_job_cold(struct a04_vendor_native_cold *);

NTSTATUS a04_vendor_native_reserve_code_cold(struct a04_vendor_native_cold *,PDMA_ADAPTER);
NTSTATUS a04_vendor_native_unmap_code_cold(struct a04_vendor_native_cold *);
NTSTATUS a04_vendor_native_validate_allocation_cold(struct a04_vendor_native_cold *,const struct a04_vendor_uncached *);
NTSTATUS a04_vendor_native_prepare_backing_cold(struct a04_vendor_native_cold *,PDMA_ADAPTER,ULONG);
NTSTATUS a04_vendor_native_unmap_output_cold(struct a04_vendor_native_cold *);
#endif
