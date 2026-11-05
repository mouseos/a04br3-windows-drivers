#ifndef MT8167_AUDIO_PNP_BACKEND_H
#define MT8167_AUDIO_PNP_BACKEND_H
#include "stream_backend.h"
#include "shared_audio.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
/* Internal prototype. The caller owns one reference, must revoke at PnP stop,
 * and release it after deregistration. No code currently calls the factory. */
struct Mt8167PnpBackend;
struct Mt8167Transport;
struct Mt8167PnpInterrupt { ULONG vector; KIRQL level; KAFFINITY affinity; KINTERRUPT_MODE mode; };
NTSTATUS Mt8167PnpBackendCreate(PDEVICE_OBJECT pdo, PRESOURCELIST resources, Mt8167PnpBackend **owner);
NTSTATUS Mt8167PnpBackendInterrupt(Mt8167PnpBackend *owner, Mt8167PnpInterrupt *interrupt);
/* Copies only validated translated IRQ coordinates. Does not connect an IRQ;
 * caller excludes concurrent transport use and holds the backend reference. */
NTSTATUS Mt8167PnpBackendBindInterrupt(Mt8167PnpBackend *owner,Mt8167Transport *transport);
/* Provider must be obtained through actual parent/child PnP, not fabricated.
 * Until a provider exists this is unused and cannot enable IRQ or DMA. */
NTSTATUS Mt8167PnpBackendAttachShared(Mt8167PnpBackend *owner,const struct mt8167_audio_shared *shared);
const Mt8167StreamBackend *Mt8167PnpBackendInterface(Mt8167PnpBackend *owner);
void Mt8167PnpBackendRevoke(Mt8167PnpBackend *owner);
void Mt8167PnpBackendRelease(Mt8167PnpBackend *owner);
#endif
#endif
