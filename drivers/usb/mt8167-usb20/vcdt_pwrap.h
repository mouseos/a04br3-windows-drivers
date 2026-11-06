/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_USB_VCDT_PWRAP_H
#define A04_USB_VCDT_PWRAP_H
#include "board_host.h"
#include "../../bus/mt8167-shared/pwrap_owner.h"
/* Claim must establish exclusive MT6392 AUXADC ownership against every
 * consumer, including firmware and secure world, and retain that lease until
 * release. The PWRAP mutex alone or a build option is not such evidence.
 * Callbacks execute under the PWRAP lock and must not reenter it. The caller
 * retains the adapter, owner and lease provider until recovery is complete;
 * a failed release must continue to exclude every other ADC consumer. */
typedef struct {
 struct a04_pwrap_owner *owner;
 void *claim_context;
 int (*claim)(void *);
 int (*release)(void *);
 void (*delay_us)(void *,unsigned);
 A04_MT6392_VCDT adc;
} A04_USB_VCDT_PWRAP;
/* -2 retains the ADC lease/quarantine; PWRAP mutex is always released. */
int A04UsbVcdtPwrapSample(A04_USB_VCDT_PWRAP *,A04_MT6392_VCDT_SAMPLE *);
#endif
