#ifndef MT8167_AFE_MAPPING_H
#define MT8167_AFE_MAPPING_H
#include <portcls.h>
/* PnP-assigned exclusive AFE registers only; SRAM is validated but not
 * mapped. Mapping itself grants no right to touch registers or connect IRQ. */
struct Mt8167AfeMapping { PVOID base; };
NTSTATUS Mt8167MapAssignedAfe(PRESOURCELIST resources,Mt8167AfeMapping *mapping);
void Mt8167UnmapAssignedAfe(Mt8167AfeMapping *mapping);
#endif
