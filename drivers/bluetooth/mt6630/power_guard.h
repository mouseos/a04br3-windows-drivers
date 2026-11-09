/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_POWER_GUARD_H
#define A04_MT6630_POWER_GUARD_H
/* A real open HCI function uses the existing whole-combo epoch even before
 * an optional SCO stream takes a hold. This is not an AFE DMA drain proof. */
static int a04_mt6630_combo_power_held(const void *hci_file,unsigned sco_holds)
{return hci_file!=0||sco_holds!=0;}
#endif
