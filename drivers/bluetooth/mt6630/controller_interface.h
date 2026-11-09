/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_CONTROLLER_INTERFACE_H
#define A04_MT6630_CONTROLLER_INTERFACE_H
#include <ntddk.h>
#include "../../bus/mt8167-shared/hfp_interface.h"
/* Ready means a real shared transport owner, never PCM/SCO qualification. */
static const GUID A04Mt6630ControllerReadyGuid={0xb33c97d1,0x4256,0x43d8,{0x98,0x71,0xe4,0x70,0x32,0xf4,0x3a,0x17}};
static const GUID A04Mt6630ControllerInterfaceGuid={0xba638e77,0x52ad,0x460a,{0xa8,0x14,0x45,0xd5,0x0b,0x92,0x20,0x10}};
struct a04_mt6630_controller_interface {INTERFACE Header;NTSTATUS (*GetController)(PVOID,struct A04_HFP_CONTROLLER *);};
#endif
