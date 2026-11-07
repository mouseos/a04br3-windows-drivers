/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_WAVERT_CACHE_H
#define MT8167_WAVERT_CACHE_H
#include <portcls.h>
#include "arm_normal_nc.h"
/* One policy for the allocating kernel alias AND AllocateAudioBuffer's
 * user-alias request. PAR validates stage 1; it is not an EL2/S2 certificate.
 * No fallback to Device/NC, no mixed-cache aliases. */
#define MT8167_WAVE_CACHE MmWriteCombined

#if defined(_ARM_) && defined(__clang__)
static inline NTSTATUS Mt8167VerifyWaveCache(PVOID mapping,ULONG bytes,ULONG firstPage)
{
    ULONG i;
    if(!mapping || ((ULONG_PTR)mapping & (PAGE_SIZE-1)) ||
       !bytes || (bytes & (PAGE_SIZE-1)) || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    for(i=0;i<bytes;i+=PAGE_SIZE) {
        ULONG savedLow,savedHigh,low,high;
        ULONG_PTR address=(ULONG_PTR)mapping+i;KIRQL old;
        /* PAR is per-CPU scratch state. Prevent migration and interrupt use
         * of PAR during this bounded query; restore PAR before lowering IRQL.
         * No page-table, MAIR, SCTLR or peripheral register is changed.
         * Only the translation operation itself runs at HIGH_LEVEL. */
        KeRaiseIrql(HIGH_LEVEL,&old);
        __asm__ volatile(
            "mrrc p15, 0, %[saveLow], %[saveHigh], c7\n"
            "mcr p15, 0, %[address], c7, c8, 0\n"
            "isb\n"
            "mrrc p15, 0, %[low], %[high], c7\n"
            "mcrr p15, 0, %[saveLow], %[saveHigh], c7\n"
            "isb\n"
            : [saveLow] "=&r"(savedLow),[saveHigh] "=&r"(savedHigh),
              [low] "=&r"(low),[high] "=&r"(high)
            : [address] "r"(address) : "memory");
        KeLowerIrql(old);
        uint64_t par=((uint64_t)high<<32)|low;
        if(!mt8167_arm_par_normal_nc(par,firstPage+i)) {
#if defined(A04_AUDIO_DIAGNOSTIC_TRACE) && A04_AUDIO_DIAGNOSTIC_TRACE
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,
                "A04AUD wave-cache rejected WC stage1 PAR=%08lx:%08lx page=%lu; no buffer access\n",
                high,low,i/PAGE_SIZE);
#endif
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }
    }
    return STATUS_SUCCESS;
}
#elif defined(A04_TEST_WAVE_CACHE) || defined(A04_TEST_MINIPORT) || defined(A04_TEST_PNP_BACKEND)
/* Host fixtures supply raw synthetic translation results explicitly.
 * This branch is never enabled by a WDK build. */
NTSTATUS Mt8167VerifyWaveCache(PVOID,ULONG,ULONG);
#else
#error WaveRT mapping validation requires the supported ARM32 Clang WDK build
#endif
#endif
