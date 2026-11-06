/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_WINDOWS_IRQ_H
#define A04_MT6630_WINDOWS_IRQ_H
#include <ntddk.h>
#if defined(A04_WIFI_IRQ_DIAGNOSTIC) && A04_WIFI_IRQ_DIAGNOSTIC
/* All control calls are PASSIVE_LEVEL and serialized with PnP and PIO.
 * MMIO and callback context remain live through disconnect and DPC flush.
 * The callback runs at DISPATCH_LEVEL and must not perform PIO or wait. */
typedef void (*a04_wifi_irq_notify)(void *);
struct a04_windows_irq {
 PKINTERRUPT interrupt; KDPC dpc; volatile UCHAR *base;
 volatile LONG active,armed; a04_wifi_irq_notify notify; void *context;
};
NTSTATUS a04_windows_irq_connect(struct a04_windows_irq *,volatile UCHAR *,
 const CM_PARTIAL_RESOURCE_DESCRIPTOR *,const CM_PARTIAL_RESOURCE_DESCRIPTOR *,a04_wifi_irq_notify,void *);
int a04_windows_irq_arm(struct a04_windows_irq *);
int a04_windows_irq_stop(struct a04_windows_irq *);
/* Surprise removal: disconnect and drain without touching device registers.
 * This does not prove the device interrupt source is masked or quiescent. */
int a04_windows_irq_remove(struct a04_windows_irq *);
#endif
#endif
