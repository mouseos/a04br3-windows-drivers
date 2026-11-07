/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_PHYSICAL_OWNER_H
#define A04_MT6630_PHYSICAL_OWNER_H
#include <ntddk.h>
/* Kernel-only METHOD_NEITHER output, caller-owned nonpaged storage through
 * synchronous completion. A referenced parent interface is not a new lease.
 * Live checks the original shared-parent generation; no NDIS code pointer is
 * returned, so adapter removal cannot leave an exported adapter callback. */
#define A04_BT_PHYSICAL_OWNER_VERSION 1u
struct a04_bt_physical_owner {INTERFACE Header;ULONG generation;int (*Live)(PVOID,ULONG);};
#endif
