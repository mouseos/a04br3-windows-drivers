/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WIFI_IRQ_FIRMWARE_H
#define A04_WIFI_IRQ_FIRMWARE_H
#include "windows_session.h"
#include "windows_irq_work.h"
#if defined(A04_WIFI_SESSION_DIAGNOSTIC) && A04_WIFI_SESSION_DIAGNOSTIC && defined(A04_WIFI_IRQ_DIAGNOSTIC) && A04_WIFI_IRQ_DIAGNOSTIC
/* One read-clear observation. The caller must provide the same non-reentrant
 * PASSIVE lock used for session start, stop, firmware and all SDIO I/O.
 * try_lock must never wait: stop holds that lock while draining this worker. */
struct a04_irq_firmware {struct a04_windows_session *session;void *context;int (*try_lock)(void *);void (*unlock)(void *);uint32_t observed;int valid;};
int a04_irq_firmware_poll(void *);
NTSTATUS a04_irq_firmware_work_init(struct a04_irq_work *,PDEVICE_OBJECT,struct a04_irq_firmware *);
#endif
#endif
