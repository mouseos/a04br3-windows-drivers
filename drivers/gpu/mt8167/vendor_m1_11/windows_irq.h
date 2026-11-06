/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_IRQ_H
#define A04_VENDOR_IRQ_H
#include <ntddk.h>
#include <wdf.h>
/* Service runs at PASSIVE_LEVEL. Return success only after sampling the FW
 * counter and preserving/processing every FWCCB request. It must hold the
 * caller's power/lifetime lock while touching GPU or DMA state. */
typedef NTSTATUS (*a04_vendor_irq_service)(void *context);
struct a04_vendor_irq;
/* Create in EvtDriverDeviceAdd, before resources are assigned. Bind MMIO in
 * EvtDevicePrepareHardware before the interrupt is enabled. */
NTSTATUS a04_vendor_irq_create(WDFDEVICE device,a04_vendor_irq_service service,
    void *context,struct a04_vendor_irq **out);
NTSTATUS a04_vendor_irq_bind(struct a04_vendor_irq *irq,volatile UCHAR *core,ULONG bytes);
/* After interrupt disconnect and work flush, before unmapping resources. */
NTSTATUS a04_vendor_irq_unbind(struct a04_vendor_irq *irq);
/* The owner enables access only while core MMIO is mapped and GPU is powered.
 * Set FALSE while holding the lifecycle lock before disabling the interrupt;
 * call flush at PASSIVE_LEVEL only after the framework disconnects the ISR. */
/* Called at PASSIVE_LEVEL with the owner's power/lifetime lock held, after
 * a successful FWCCB snapshot. Never rearms an asserted/unknown source. */
BOOLEAN a04_vendor_irq_rearm(struct a04_vendor_irq *irq);
void a04_vendor_irq_set_power(struct a04_vendor_irq *irq,BOOLEAN powered);
void a04_vendor_irq_kick(struct a04_vendor_irq *irq);
void a04_vendor_irq_flush(struct a04_vendor_irq *irq);
#endif
