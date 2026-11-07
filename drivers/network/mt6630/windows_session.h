/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_WIFI_SESSION_H
#define A04_WINDOWS_WIFI_SESSION_H
#include "windows_diagnostic.h"
/* Diagnostic plumbing only. All calls at PASSIVE_LEVEL under the same PnP/I/O
 * serialization. The referenced parent must retain the RTC/combo lease; the
 * platform must own a translated IRQ and mask it before PIO is enabled,
 * and must keep its context valid until stop has completed. */
#if defined(A04_WIFI_SESSION_DIAGNOSTIC) && A04_WIFI_SESSION_DIAGNOSTIC
struct a04_windows_session_platform {
 void *context;
 int (*connect_masked_irq)(void *,struct a04_windows_msdc2 *);
 int (*mask_and_drain_irq)(void *);
 int (*drain_io)(void *);
 /* Called after IRQ and I/O drain, before unmap/release. Must establish that
  * combo and firmware no longer use leased resources; a mask alone is not
  * proof. Failure retains the mapping and lease. No SDIO after life stop. */
 int (*quiesce_combo)(void *);
};
enum a04_windows_session_state { A04_SESSION_OFF,A04_SESSION_STARTING,A04_SESSION_READY,A04_SESSION_STOPPED,A04_SESSION_QUARANTINED,A04_SESSION_REJECTED };
struct a04_windows_session {
 enum a04_windows_session_state state;
 A04_SHARED_WIFI_INTERFACE *parent;
 struct a04_windows_session_platform platform;
 struct a04_wifi_owner owner;
 struct a04_windows_msdc2 host;
 struct a04_windows_diagnostic diagnostic;
 struct a04_wifi_life_ops life_ops;
 ULONG generation;
};
int a04_windows_session_start(struct a04_windows_session *,A04_SHARED_WIFI_INTERFACE *,
 const struct a04_windows_session_platform *,const CM_PARTIAL_RESOURCE_DESCRIPTOR *,
 const CM_PARTIAL_RESOURCE_DESCRIPTOR *,uint32_t,const uint8_t [32],
 const struct a04_wmt_startup_options *);
/* Optional observation-only init tracing. The old entry keeps identical
 * behavior and passes NULL; callback executes only on the checked PASSIVE path. */
typedef void (*a04_session_trace_fn)(void *,unsigned,unsigned,unsigned,ULONG,ULONG);
int a04_windows_session_start_traced(struct a04_windows_session *,A04_SHARED_WIFI_INTERFACE *,
 const struct a04_windows_session_platform *,const CM_PARTIAL_RESOURCE_DESCRIPTOR *,
 const CM_PARTIAL_RESOURCE_DESCRIPTOR *,uint32_t,const uint8_t [32],
 const struct a04_wmt_startup_options *,a04_session_trace_fn,void *);
int a04_windows_session_stop(struct a04_windows_session *);
#endif
#endif
