/* SPDX-License-Identifier: MIT */
#ifndef A04_NATIVE_ACTIVATION_POLICY_H
#define A04_NATIVE_ACTIVATION_POLICY_H
/* Called while the actual provider lifetime lock is held. No fabricated
 * display-copy lease/fence is generated to admit a diagnostic. The pinned
 * copy owner must be ours and no display client may own an active lease. */
static inline int a04_native_activation_allowed(int passive,int already_attempted,
    int gp_fence_ok,int native_prepared,int pinned_copy_owner,int service_online,int service_closed,
    int display_lease_active,int display_owner_present)
{
    return passive && !already_attempted && gp_fence_ok && native_prepared && pinned_copy_owner &&
        !service_online && !service_closed && !display_lease_active && !display_owner_present;
}
#endif
