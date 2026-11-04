#ifndef MT8167_AUDIO_PARENT_CLIENT_H
#define MT8167_AUDIO_PARENT_CLIENT_H
#include "../../bus/mt8167-shared/audio_interface.h"
#include "shared_audio.h"
/* Adapter for an already successful IRP_MN_QUERY_INTERFACE. The input owns
 * one query reference; bind transfers it to this object on success only.
 * Object must outlive all attached shared_audio clients. No PnP query occurs
 * here. Caller serializes bind/get/close with PnP stop and removal. */
struct Mt8167AudioParentClient {
    A04_SHARED_AUDIO_INTERFACE parent;
    struct mt8167_audio_shared service;
    int bound;
    unsigned clients;
};
#ifdef __cplusplus
extern "C" {
#endif
int Mt8167AudioParentBind(struct Mt8167AudioParentClient *,const A04_SHARED_AUDIO_INTERFACE *);
const struct mt8167_audio_shared *Mt8167AudioParentService(struct Mt8167AudioParentClient *);
int Mt8167AudioParentClose(struct Mt8167AudioParentClient *);
#ifdef __cplusplus
}
#endif
#endif
