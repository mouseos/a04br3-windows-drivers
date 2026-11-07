/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_OWNER_FACTORY_PRIVATE_H
#define A04_NATIVE_OWNER_FACTORY_PRIVATE_H
#include "../wddm-render/native_binding.h"
#include "../../../bus/mt8167-shared/gpu_platform_interface.h"
#include "native_owner_allocation_private.h"
#include "native_owner_frontend_resolver.h"
#include "windows_command.h"
#include "windows_stop.h"
#define E_FACTORY_TAG 0x664e4741u
#define E_FIRMWARE_TAG 0x704e4741u
#define E_OWNER_ACTIVE 1u
#define E_OWNER_QUIESCED 2u
#define E_OWNER_FAILED 3u
#define E_JOB_NONE 0u
#define E_JOB_PREPARED 1u
#define E_JOB_SUBMITTED 2u
#define E_JOB_COMPLETE 3u
struct e_native_owner {
    DXGKRNL_INTERFACE dxgk;struct geo_frontend_memory memory;
    struct a04_native_owner_allocation *allocation;
    volatile UCHAR *core,*mfg;ULONG irq_level;
    PFILE_OBJECT parent_file;A04_SHARED_GPU_DOMAIN_INTERFACE platform;
    ULONGLONG platform_token;BOOLEAN platform_referenced,platform_held;
    void *firmware_image;ULONG firmware_bytes;
    KMUTEX mutex;EX_RUNDOWN_REF operations;
    volatile LONG references,closing,irq_gate;BOOLEAN rundown_closed,leased;
    uint32_t state,power,job_state,colour,kick_write,generation;
    uint64_t epoch,hardware_epoch,next_id,surface,job,aperture_offset;
    struct geo_surface_desc description,vertex_description;
    uint64_t vertex,vertex_aperture_offset;
    struct a04_native_irq_off_observation off_irq;
    struct a04_rgx_core_reset_observation reset;
    BOOLEAN hardware_off,receipt_valid;
};
/* Real platform discovery/query; holds file + referenced interface + D0 lease. */
NTSTATUS e_native_platform_open(struct e_native_owner *);
NTSTATUS e_native_platform_close(struct e_native_owner *);
NTSTATUS e_native_platform_on(struct e_native_owner *);
NTSTATUS e_native_platform_observe(struct e_native_owner *,struct a04_gpu_domain_snapshot *);
NTSTATUS e_native_load_firmware(struct e_native_owner *);
NTSTATUS e_native_map_resources(struct e_native_owner *,PDEVICE_OBJECT,const DXGK_DEVICE_INFO *);
BOOLEAN e_native_synchronize(void *,PKSYNCHRONIZE_ROUTINE,void *);
NTSTATUS e_native_stop_locked(struct e_native_owner *);
NTSTATUS e_native_retire_graph_locked(struct e_native_owner *);
NTSTATUS e_native_build_graph_locked(struct e_native_owner *);
NTSTATUS e_native_boot_locked(struct e_native_owner *);
#endif
