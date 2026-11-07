/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WIFI_IRQ_WORK_H
#define A04_WIFI_IRQ_WORK_H
#include <ntddk.h>
#if defined(A04_WIFI_IRQ_DIAGNOSTIC) && A04_WIFI_IRQ_DIAGNOSTIC
/* process runs at PASSIVE_LEVEL and must serialize its own firmware/PIO work.
 * The work item, context and device live until IRQ has stopped and drain ends. */
struct a04_irq_work { PIO_WORKITEM work; KEVENT idle; volatile LONG busy,closing,fault; int (*process)(void *);void *context; };
NTSTATUS a04_irq_work_init(struct a04_irq_work *,PDEVICE_OBJECT,int (*)(void *),void *);
void a04_irq_work_notify(void *);
int a04_irq_work_drain(struct a04_irq_work *);
#endif
#endif
