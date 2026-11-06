/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GE8300_STATUS_H
#define A04_GE8300_STATUS_H
#include <stdint.h>
#include <stddef.h>
#define A04_GE8300_IOCTL_QUERY 0x00226004u
/* CPU-only vendor FW staging; does not indicate a GPU boot. */
#define A04_GE8300_IOCTL_VENDOR_STAGE_QUERY 0x00226010u
struct a04_ge8300_vendor_stage_status { unsigned long version, attempted, stage_status; };
/* Read-only ACPI snapshot; success is not power ownership or GPU start. */
#define A04_GE8300_IOCTL_VENDOR_POWER_QUERY 0x00226014u
struct a04_ge8300_vendor_power_status {
    unsigned long version, attempted, status;
    unsigned long spm_status, spm_status_2, mfg_clock_status;
    unsigned long top_mux0, top_mux1, top_slow;
};
/* METHOD_BUFFERED: exactly one 16 KiB input/output transfer. Kernel callers
 * only; provider-side enforcement is mandatory. Experimental opt-in. */
#define A04_GE8300_IOCTL_COPY 0x00226008u
#define A04_GE8300_COPY_BYTES 16384u
/* WinPE only, METHOD_BUFFERED 4 KiB private m1.11 bounce transfer. */
#define A04_GE8300_IOCTL_VENDOR_COPY 0x00226018u
#define A04_GE8300_VENDOR_COPY_BYTES 4096u
/* Kernel-only experimental bounded copy with local GPU completion evidence. */
#define A04_GE8300_IOCTL_VENDOR_FENCED_COPY 0x0022601cu
struct a04_ge8300_fenced_copy {
    uint32_t version, task_sequence, flush_sequence;
    uint8_t pixels[A04_GE8300_VENDOR_COPY_BYTES];
};
/* Diagnostic-only report from the administrator-owned WinPE status tool. */
/* Internal kernel-only lifetime lease; opaque cookie, never for user mode. */
#define A04_GE8300_IOCTL_SHARED_ACQUIRE 0x00226020u
#define A04_GE8300_IOCTL_SHARED_RELEASE 0x00226024u
#define A04_GE8300_IOCTL_SHARED_STAGE 0x00226028u
#define A04_GE8300_IOCTL_SHARED_UNSTAGE 0x0022602cu
#define A04_GE8300_IOCTL_SHARED_DIAGNOSTIC 0x00226030u
#define A04_GE8300_IOCTL_SHARED_TILE 0x00226034u
#define A04_GE8300_IOCTL_SHARED_COMMON_PAGE 0x00226038u
/* Kernel-only, short-lived lease on the GPU common-buffer copy result. */
struct a04_ge8300_common_page { void *cpu; uint64_t gpu_va; uint32_t token,task,flush; void *gpu_adapter,*gpu_pdo; };
#if defined(_ARM_)
#ifdef __cplusplus
static_assert(sizeof(struct a04_ge8300_common_page)==40, "common page ABI");
#else
_Static_assert(sizeof(struct a04_ge8300_common_page)==40, "common page ABI");
#endif
#endif
/* METHOD_BUFFERED, kernel callers; no display visibility claim. */
struct a04_ge8300_shared_tile { uint32_t token,frame,index,task,flush; uint32_t pixels[1024]; };
#if defined(_ARM_)
#ifdef __cplusplus
static_assert(sizeof(struct a04_ge8300_shared_tile)==4116, "shared tile ABI");
#else
_Static_assert(sizeof(struct a04_ge8300_shared_tile)==4116, "shared tile ABI");
#endif
#endif
struct a04_ge8300_shared_lease {
    void *gpu_adapter, *gpu_pdo, *shared_mdl_owner;
    uint64_t inactive_gpu_pc_dma;
    uint32_t token, task_sequence, flush_sequence;
    uint32_t diagnostic_task, diagnostic_flush, diagnostic_matches;
    uint32_t ovl_enable, ovl_sources, ovl_l0_control, ovl_l0_size;
    uint32_t ovl_l0_pitch, ovl_l0_address, ovl_interrupt_status;
};
#if defined(_ARM_)
#ifdef __cplusplus
static_assert(sizeof(struct a04_ge8300_shared_lease)==80 && offsetof(struct a04_ge8300_shared_lease,inactive_gpu_pc_dma)==16, "shared lease ABI");
#else
_Static_assert(sizeof(struct a04_ge8300_shared_lease)==80 && offsetof(struct a04_ge8300_shared_lease,inactive_gpu_pc_dma)==16, "shared lease ABI");
#endif
#endif
#define A04_GE8300_IOCTL_REPORT_DISPLAY 0x0022600cu
struct a04_ge8300_status {
    unsigned long version, attempted, boot_status, health_status, shutdown_status;
    unsigned long bvnc_low, bvnc_high, processor_released, firmware_started, queue_faulted;
    unsigned long sync_error, queue_read, queue_write, queue_return, power_sync;
    unsigned long connection_fw, connection_os, fw_power_state, commands_executed;
    unsigned long mts_enable, mts_bg, mts_int, mts_count_low, mts_count_high;
    unsigned long trace_pointer, trace_words[4];
    unsigned long mmu_status;
    unsigned long queue_status;
    unsigned long copy_completed, copy_failed;
};
#endif
