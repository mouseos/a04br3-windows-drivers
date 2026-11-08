/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_TOPOLOGY_MINIPORT_H
#define MT8167_TOPOLOGY_MINIPORT_H
#include <portcls.h>
NTSTATUS Mt8167NewCaptureTopology(PMINIPORTTOPOLOGY *out);
NTSTATUS Mt8167NewTopology(PMINIPORTTOPOLOGY *out,PDEVICE_OBJECT device=NULL);
#endif
