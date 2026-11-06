#ifndef MT8167_AUDIO_PARENT_QUERY_H
#define MT8167_AUDIO_PARENT_QUERY_H
#include "audio_parent_client.h"
/* PASSIVE_LEVEL; target is the actual child PDO. On success out owns exactly
 * one interface reference. On failure out is all-zero and no reference is
 * transferred. No timeout/cancellation: a misbehaving lower stack can hang. */
#ifdef __cplusplus
extern "C" {
#endif
NTSTATUS Mt8167QueryAudioParent(PDEVICE_OBJECT target,A04_SHARED_AUDIO_INTERFACE *out);
#ifdef __cplusplus
}
#endif
#endif
