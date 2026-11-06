/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GE8300_STATUS_H
#define A04_GE8300_STATUS_H
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
/* WinPE diagnostic: close submissions and attempt FW cleanup/power-off.
 * Success does not permit DMA reclamation or PnP unload. */
#define A04_GE8300_IOCTL_VENDOR_STOP 0x0022601cu
/* Diagnostic-only report from the administrator-owned WinPE status tool. */
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
