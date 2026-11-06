/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_DISPLAY_RESOURCES_HXX
#define A04_DISPLAY_RESOURCES_HXX
#ifndef A04_GE8300_WDDM_EXPLICIT_GPU_FLUSH
#define A04_GE8300_WDDM_EXPLICIT_GPU_FLUSH 0
#endif
#ifndef A04_GE8300_WDDM_OVL_OBSERVE
#define A04_GE8300_WDDM_OVL_OBSERVE 0
#endif
#ifndef A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
#define A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC 0
#endif
#ifndef A04_GE8300_WDDM_DIRECT_EXPERIMENT
#define A04_GE8300_WDDM_DIRECT_EXPERIMENT 0
#endif
#ifndef A04_GE8300_WDDM_SHARED_DIAGNOSTIC
#define A04_GE8300_WDDM_SHARED_DIAGNOSTIC 0
#endif
#ifndef A04_GE8300_WDDM_SHARED_FULL_COMMON
#define A04_GE8300_WDDM_SHARED_FULL_COMMON 0
#endif
#ifndef A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC
#define A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC 0
#endif
#ifndef A04_GE8300_WDDM_GCE_LEASE
#define A04_GE8300_WDDM_GCE_LEASE 0
#endif
#if A04_GE8300_WDDM_EXPLICIT_GPU_FLUSH && !A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC
#error Explicit GPU DMA flush requires the common frame diagnostic
#endif
#if A04_GE8300_WDDM_OVL_OBSERVE && !A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC
#error OVL observation requires the common frame diagnostic
#endif
#if A04_GE8300_WDDM_GCE_LEASE && !A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC
#error GCE lease diagnostic requires the common frame diagnostic
#endif
#if A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC && !A04_GE8300_WDDM_SHARED_FULL_COMMON
#error Common frame diagnostic requires the full common probe
#endif
#if A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC && !A04_GE8300_SHARED_DISPLAY_COMMON
#error Common frame diagnostic requires shared display common support
#endif
#ifndef A04_GE8300_WDDM_SHARED_COMMON_PAGE
#define A04_GE8300_WDDM_SHARED_COMMON_PAGE 0
#endif
#if A04_GE8300_WDDM_SHARED_FULL_COMMON && !A04_GE8300_WDDM_SHARED_COMMON_PAGE
#error Full common buffer probe requires shared common page
#endif
#if A04_GE8300_WDDM_SHARED_COMMON_PAGE && !A04_GE8300_WDDM_SHARED_DIAGNOSTIC
#error Shared common page requires shared diagnostic
#endif
#ifndef A04_GE8300_WDDM_SHARED_PRESENT
#define A04_GE8300_WDDM_SHARED_PRESENT 0
#endif
#if A04_GE8300_WDDM_SHARED_PRESENT && !A04_GE8300_WDDM_SHARED_DIAGNOSTIC
#error Shared present requires shared diagnostic
#endif
#ifndef A04_GE8300_WDDM_SHARED_AUTOSTART
#define A04_GE8300_WDDM_SHARED_AUTOSTART 0
#endif
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC && (!A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC || !A04_GE8300_WDDM_SHARED_AUTOSTART)
#error GPU POST diagnostic requires the common frame worker
#endif
#if A04_GE8300_WDDM_SHARED_AUTOSTART && !A04_GE8300_WDDM_SHARED_DIAGNOSTIC
#error Shared autostart requires shared diagnostic
#endif
#ifndef A04_GE8300_WDDM_SHARED_LEASE
#define A04_GE8300_WDDM_SHARED_LEASE 0
#endif
#if A04_GE8300_WDDM_SHARED_DIAGNOSTIC && !A04_GE8300_WDDM_SHARED_LEASE
#error Shared diagnostic requires WDDM shared lease
#endif
#if A04_GE8300_WDDM_SHARED_LEASE && !A04_GE8300_WDDM_DIRECT_EXPERIMENT
#error Shared lease requires WDDM direct experiment
#endif
#if A04_GE8300_WDDM_DIRECT_EXPERIMENT
#include "../ge8300_status.h"
#include "shared_common_policy.h"
#include "../vendor_m1_11/windows_shared_scanout.h"
#include "../vendor_m1_11/windows_shared_ovl.h"
struct A04DisplayResources {
    PDMA_ADAPTER Adapter;
    volatile ULONG *Ovl;
    ULONG MapRegisters;
    BOOLEAN HasInterrupt;
    PDEVICE_OBJECT DisplayPdo;
    struct a04_shared_scanout Shared;
    struct a04_shared_ovl SharedOvl;
#if A04_GE8300_WDDM_SHARED_LEASE
    HANDLE GpuHandle;
    struct a04_ge8300_shared_lease Lease;
#if A04_GE8300_WDDM_SHARED_PRESENT
    uint32_t FrameId;
    NTSTATUS SubmitFullFrame(const void *pixels,uint32_t pitch);
#endif
    NTSTATUS PrepareViaGpu();
#if A04_GE8300_WDDM_SHARED_COMMON_PAGE
    BOOLEAN SharedCommonRetained;
    BOOLEAN SharedCommonFinished;
    NTSTATUS SharedCommonResult;
    void *SharedCommonOwner;
#if A04_GE8300_WDDM_GCE_LEASE
    void *GceDisplayLease;
#if A04_GE8300_WDDM_EXPLICIT_GPU_FLUSH
    BOOLEAN GceFrameAttempted;
    /* Caller holds the present lock and owns the stop event and proof context.
     * Provider must independently prove both saved display mappings, the
     * GPU-to-display visibility and the selected OVL geometry. */
    NTSTATUS GcePresentAndRestore(ULONG event,ULONG savedL0,ULONG savedL3,
        int (*validate)(void *,uint32_t,uint32_t),void *proofContext,PKEVENT stopEvent);
#endif
#endif
    struct a04_shared_common_once SharedCommonOnce;
#if A04_GE8300_WDDM_SHARED_FULL_COMMON
    NTSTATUS ProbeDisplayFullCommon(struct a04_ge8300_common_page *page);
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
    NTSTATUS GetVerifiedGpuFrame(const uint32_t **pixels);
#endif
#endif
    NTSTATUS ProbeSharedCommonPage();
#endif
    /* No call site until platform visibility and frame callbacks exist. */
    NTSTATUS SwitchShared(uint32_t expected,a04_shared_ovl_ready ready,
        a04_shared_ovl_boundary boundary,void *context);
#endif
    /* Acquires only. No GPU memory is shared and no IRQ/MMIO writes occur. */
    NTSTATUS Start(PDEVICE_OBJECT pdo, const DXGK_DEVICE_INFO *info);
    /* Only when the GPU owner holds its PDO/adapter lease through Stop. */
    NTSTATUS PrepareShared(PDMA_ADAPTER gpu, PDEVICE_OBJECT gpuPdo);
    VOID Stop();
};
#endif
#endif
