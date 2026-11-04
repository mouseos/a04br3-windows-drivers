/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MSDC_MULTIBLOCK_CONFIG_H
#define A04_MSDC_MULTIBLOCK_CONFIG_H
/* Set to 1 only for a dedicated, read-only diagnostic build. */
#ifndef A04_DMA_MULTIBLOCK_EXPERIMENT
#define A04_DMA_MULTIBLOCK_EXPERIMENT 0
#endif
/* Separate opt-in for the guarded, isolated boot prototype. */
#ifndef A04_BOOT_MULTIBLOCK_EXPERIMENT
#define A04_BOOT_MULTIBLOCK_EXPERIMENT 0
#endif
#if A04_BOOT_MULTIBLOCK_EXPERIMENT && (!defined(A04_BOOT_PASSIVE_EXPERIMENT) || !defined(A04_DMA_PROBE) || !defined(A04_READONLY_DISK_PROBE) || defined(A04_DMA_WRITE_TEST) || A04_DMA_MULTIBLOCK_EXPERIMENT)
#error Boot multiblock requires the isolated passive boot configuration and excludes the read-only experiment
#endif
#define A04_ANY_MULTIBLOCK (A04_DMA_MULTIBLOCK_EXPERIMENT || A04_BOOT_MULTIBLOCK_EXPERIMENT)
#endif
