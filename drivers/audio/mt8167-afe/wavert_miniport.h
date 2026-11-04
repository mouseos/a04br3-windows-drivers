#ifndef MT8167_WAVERT_MINIPORT_H
#define MT8167_WAVERT_MINIPORT_H
#include <portcls.h>
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
/* PASSIVE_LEVEL. Software diagnostic only; stream RUN is rejected. */
NTSTATUS Mt8167NewMiniport(PMINIPORTWAVERT *out);
#endif
#endif
