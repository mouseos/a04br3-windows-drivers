/* SPDX-License-Identifier: MIT */
#ifndef A04_LC709203F_BATTERY_CLASS_H
#define A04_LC709203F_BATTERY_CLASS_H
#include <ntddk.h>
#include <wdf.h>
#include <batclass.h>
#include "gauge.h"

/* Owner creates the wait lock. Invalidate on D0 exit and failed/stale reads.
 * A TRUE return from publish/invalidate requests a class status notification;
 * the owner must coordinate that notification with its class-handle lifetime. */
struct a04_battery_class {
    WDFWAITLOCK lock;
    struct a04_gauge_sample sample;
    ULONG tag;
    BOOLEAN valid;
    ULONG power_state;
    /* Immutable callback/context set at AddDevice, only on the local read-only
     * owner path. QueryInformation must release class lock before calling. */
    PVOID temperature_context;
    NTSTATUS (*read_temperature)(PVOID context, PULONG kelvin10);
};
BOOLEAN a04_battery_publish(struct a04_battery_class *context,
                            const struct a04_gauge_sample *sample);
BOOLEAN a04_battery_publish_power(struct a04_battery_class *context,
                                  const struct a04_gauge_sample *sample, ULONG power_state);
BOOLEAN a04_battery_invalidate(struct a04_battery_class *context);
BCLASS_QUERY_TAG_CALLBACK a04_battery_query_tag;
BCLASS_QUERY_INFORMATION_CALLBACK a04_battery_query_information;
BCLASS_QUERY_STATUS_CALLBACK a04_battery_query_status;
BCLASS_SET_INFORMATION_CALLBACK a04_battery_set_information;
BCLASS_SET_STATUS_NOTIFY_CALLBACK a04_battery_set_notify;
BCLASS_DISABLE_STATUS_NOTIFY_CALLBACK a04_battery_disable_notify;
#endif
