/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_USB_OWNER_H
#define A04_MT8167_USB_OWNER_H
#include "pwrap_owner.h"
#include "../../usb/mt8167-usb20/gpio22.h"
/* Parent serializes PWRAP, GPIO and top-clock operations with all clients.
 * verify_upstream must establish UNIVPLL/USB mux and clock dependencies from
 * owned platform resources; mappings alone are not sufficient. */
struct a04_usb_owner {
 void *context; int (*lock)(void *); void (*unlock)(void *);
 /* Optional read-only eligibility probe. Rejection is temporary and occurs
  * before GPIO/clock/PMIC changes; it must not poison the shared parent. */
 int (*probe_source)(void *);
 int (*verify_upstream)(void *);
 int (*verify_overcurrent)(void *);
 /* Charger/source owner serializes its watchdog and external source. -1 is
  * rejection before effects; other failures mean ownership is uncertain. */
 int (*otg_acquire)(void *); int (*otg_release)(void *);
 unsigned (*top_read)(void *,unsigned); void (*top_write)(void *,unsigned,unsigned);
 struct a04_pmic_read *pmic; A04_GPIO_IO gpio;
 int online,leased,poisoned,otg_held;
};
int a04_usb_owner_acquire(struct a04_usb_owner *);
int a04_usb_owner_vbus(struct a04_usb_owner *,int);
int a04_usb_owner_health(struct a04_usb_owner *);
int a04_usb_owner_release(struct a04_usb_owner *);
int a04_usb_owner_stop(struct a04_usb_owner *);
#endif
