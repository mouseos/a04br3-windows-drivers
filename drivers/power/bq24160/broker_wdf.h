/* SPDX-License-Identifier: MIT */
#ifndef A04_BROKER_WDF_H
#define A04_BROKER_WDF_H
#include <ntddk.h>
#include <wdf.h>
#include "broker_client.h"
/* Not a discovery interface. The parent must provide a validated owner target,
 * an agreed IOCTL, same-boot epoch, shared timebase and a passive WDFWAITLOCK.
 * It must hold target/lock lifetimes until all calls have drained. This module
 * does not claim I2C, establish an owner, or authorize charger access. */
struct a04_broker_wdf {
    WDFIOTARGET target;
    WDFWAITLOCK lock;
    ULONG ioctl;
    struct a04_broker_client client;
};
int a04_broker_wdf_init(struct a04_broker_wdf *, WDFIOTARGET, WDFWAITLOCK, ULONG ioctl, uint64_t epoch);
int a04_broker_wdf_read(struct a04_broker_wdf *, uint64_t deadline_ns, struct a04_gauge_sample *);
void a04_broker_wdf_stop(struct a04_broker_wdf *);
#endif
