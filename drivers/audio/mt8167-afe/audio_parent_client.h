#ifndef MT8167_AUDIO_PARENT_CLIENT_H
#define MT8167_AUDIO_PARENT_CLIENT_H
#include "../../bus/mt8167-shared/audio_interface.h"
#include "shared_audio.h"
#if defined(A04_AUDIO_LIFECYCLE) && A04_AUDIO_LIFECYCLE
#include "../../bus/mt8167-shared/audio_lifecycle_interface.h"
#endif
/* Adapter for an already successful IRP_MN_QUERY_INTERFACE. The input owns
 * one query reference; bind transfers it to this object on success only.
 * Object must outlive all attached shared_audio clients. No PnP query occurs
 * here. Caller serializes bind/get/close with PnP stop and removal. */
struct Mt8167AudioParentClient {
    A04_SHARED_AUDIO_INTERFACE parent;
    struct mt8167_audio_shared service;
    int bound;
    unsigned clients;
#if defined(A04_AUDIO_LIFECYCLE) && A04_AUDIO_LIFECYCLE
    struct A04_AUDIO_LIFECYCLE_INTERFACE lifecycle;
    ULONG generation;
    int lifecycleBound,softwareQuiescent;
#endif
};
#ifdef __cplusplus
extern "C" {
#endif
int Mt8167AudioParentBind(struct Mt8167AudioParentClient *,const A04_SHARED_AUDIO_INTERFACE *);
const struct mt8167_audio_shared *Mt8167AudioParentService(struct Mt8167AudioParentClient *);
int Mt8167AudioParentClose(struct Mt8167AudioParentClient *);
#if defined(A04_AUDIO_LIFECYCLE) && A04_AUDIO_LIFECYCLE
int Mt8167AudioParentBindLifecycle(struct Mt8167AudioParentClient *,const struct A04_AUDIO_LIFECYCLE_INTERFACE *);
NTSTATUS Mt8167AudioParentCertifyStopped(struct Mt8167AudioParentClient *);
NTSTATUS Mt8167AudioParentReleaseStopped(struct Mt8167AudioParentClient *);
#endif
#ifdef __cplusplus
}
#endif
#endif
