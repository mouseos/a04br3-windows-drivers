/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_SESSION_IRQ_H
#define A04_MT6630_SESSION_IRQ_H
#include "windows_session.h"
#include "windows_irq.h"
#include "windows_irq_work.h"
#if defined(A04_WIFI_SESSION_DIAGNOSTIC) && A04_WIFI_SESSION_DIAGNOSTIC && defined(A04_WIFI_IRQ_DIAGNOSTIC) && A04_WIFI_IRQ_DIAGNOSTIC
/* Caller retains descriptors and context. MSDC2 is mapped before the provider masks and connects IRQ; drain_io must drain any work queued by the
 * interrupt notification, including executing work. Both run at PASSIVE_LEVEL. */
struct a04_windows_session_irq {
 struct a04_windows_irq irq;
 const CM_PARTIAL_RESOURCE_DESCRIPTOR *raw,*translated;
 struct a04_irq_work *work;void *context;
 int (*drain_io)(void *);
 int (*quiesce_combo)(void *);
};
int a04_windows_session_irq_bind(struct a04_windows_session_irq *,struct a04_windows_session_platform *);
#endif
#endif
