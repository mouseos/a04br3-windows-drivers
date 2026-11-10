#ifndef MT8167_AUDIO_PARENT_QUERY_H
#define MT8167_AUDIO_PARENT_QUERY_H
#include "audio_parent_client.h"
/* PASSIVE_LEVEL; actual PDO. Success transfers one query reference.
 * A timeout is never interpreted as query or hardware completion. */
#ifdef __cplusplus
extern "C" {
#endif
NTSTATUS Mt8167QueryAudioParent(PDEVICE_OBJECT,A04_SHARED_AUDIO_INTERFACE *);
#if defined(A04_AUDIO_LIFECYCLE) && A04_AUDIO_LIFECYCLE
NTSTATUS Mt8167QueryAudioLifecycle(PDEVICE_OBJECT,struct A04_AUDIO_LIFECYCLE_INTERFACE *);
#endif
#ifdef __cplusplus
}
#endif
#endif
