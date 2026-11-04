/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_TOPRGU_HANDOFF_POLICY_H
#define A04_TOPRGU_HANDOFF_POLICY_H
/* Only the targeted DPC on logical CPU1 can witness that CPU1 is executing
 * Windows, after the board firmware's final kick and parking handoff. This is
 * conditional on the documented four-CPU board topology and boot shim. */
static int a04_toprgu_can_claim(unsigned active, int backup, unsigned current_cpu, unsigned mpidr_affinity)
{ return active == 4 && backup && current_cpu == 1 && mpidr_affinity == 1; }
static int a04_toprgu_can_feed(unsigned active, int claimed)
{ return active == 4 && claimed; }
#endif
