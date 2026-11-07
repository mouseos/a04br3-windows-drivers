/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_HFP_POWER_H
#define MT8167_HFP_POWER_H
#include <portcls.h>
struct Mt8167PnpBackend;
struct Mt8167HfpPower;
NTSTATUS Mt8167HfpPowerCreate(PDEVICE_OBJECT,Mt8167PnpBackend *,PRESOURCELIST,Mt8167HfpPower **);
NTSTATUS Mt8167HfpPowerStop(Mt8167HfpPower *);
void Mt8167HfpPowerRelease(Mt8167HfpPower *);
#endif
