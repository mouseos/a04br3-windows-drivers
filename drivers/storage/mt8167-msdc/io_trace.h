/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MSDC_IO_TRACE_H
#define A04_MSDC_IO_TRACE_H
/* Only routine request chatter. DMA poll, failure, recovery and lifecycle
 * diagnostics deliberately do not depend on this switch. */
#ifndef A04_MSDC_VERBOSE_IO_TRACE
#define A04_MSDC_VERBOSE_IO_TRACE 0
#endif
#if A04_MSDC_VERBOSE_IO_TRACE != 0 && A04_MSDC_VERBOSE_IO_TRACE != 1
#error A04_MSDC_VERBOSE_IO_TRACE must be 0 or 1
#endif
#endif
