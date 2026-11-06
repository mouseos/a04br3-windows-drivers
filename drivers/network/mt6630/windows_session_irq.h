/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_SESSION_IRQ_H
#define A04_MT6630_SESSION_IRQ_H
#include "windows_session.h"
#include "windows_irq.h"
#include "windows_irq_work.h"
#if defined(A04_WIFI_SESSION_DIAGNOSTIC) && A04_WIFI_SESSION_DIAGNOSTIC && defined(A04_WIFI_IRQ_DIAGNOSTIC) && A04_WIFI_IRQ_DIAGNOSTIC
/* Caller retains descriptors and context. pre_mask must independently establish
 * source quiescence before mapping; drain_io must drain any work queued by the
 * interrupt notification, including executing work. Both run at PASSIVE_LEVEL. */
struct a04_windows_session_irq {
 struct a04_windows_irq irq;
 const CM_PARTIAL_RESOURCE_DESCRIPTOR *raw,*translated;
 struct a04_irq_work *work;void *context;
 int (*pre_mask)(void *);int (*drain_io)(void *);
};
int a04_windows_session_irq_bind(struct a04_windows_session_irq *,struct a04_windows_session_platform *);
#endif
#endif
