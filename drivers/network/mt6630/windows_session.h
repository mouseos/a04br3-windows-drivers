/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_WIFI_SESSION_H
#define A04_WINDOWS_WIFI_SESSION_H
#include "windows_diagnostic.h"
/* Diagnostic plumbing only. All calls at PASSIVE_LEVEL under the same PnP/I/O
 * serialization. The referenced parent must retain the RTC/combo lease; the
 * platform must own a translated IRQ, have it masked and drained before map,
 * and must keep its context valid until stop has completed. */
#if defined(A04_WIFI_SESSION_DIAGNOSTIC) && A04_WIFI_SESSION_DIAGNOSTIC
struct a04_windows_session_platform {
 void *context;
 int (*prepare_masked_irq)(void *);
 int (*connect_masked_irq)(void *,struct a04_windows_msdc2 *);
 int (*mask_and_drain_irq)(void *);
 int (*drain_io)(void *);
};
enum a04_windows_session_state { A04_SESSION_OFF,A04_SESSION_STARTING,A04_SESSION_READY,A04_SESSION_STOPPED,A04_SESSION_QUARANTINED };
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
int a04_windows_session_stop(struct a04_windows_session *);
#endif
#endif
