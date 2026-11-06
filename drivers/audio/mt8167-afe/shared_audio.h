#ifndef MT8167_SHARED_AUDIO_H
#define MT8167_SHARED_AUDIO_H
#include <stdint.h>
/* Internal audio client contract, not a published WDF/ACPI interface.
 * The future single parent controls admission under its PnP lock and owns
 * all PWRAP/APMIXED/TOPCKGEN accesses. Lease includes validated board codec
 * mute and clock configuration; no raw PMIC operations are exposed here.
 * Parent lifetime is held from reference until dereference. A lease cannot
 * be released until device DMA and IRQ are independently known quiescent. */
struct mt8167_audio_shared {
    uint32_t size,version;
    void *context;
    int (*reference)(void *);
    void (*dereference)(void *);
    /* -1: no side effects; other failures: ownership state is uncertain. */
    int (*acquire_muted)(void *,uint32_t rate);
    int (*release_quiescent)(void *);
};
struct mt8167_audio_shared_client {
    struct mt8167_audio_shared service;
    int referenced,leased,quarantined;
};
#ifdef __cplusplus
extern "C" {
#endif
int mt8167_audio_shared_attach(struct mt8167_audio_shared_client *,const struct mt8167_audio_shared *);
int mt8167_audio_shared_acquire(struct mt8167_audio_shared_client *,uint32_t rate);
/* Unexposed is permitted only before any AFE write capable of starting DMA. */
int mt8167_audio_shared_release_unexposed(struct mt8167_audio_shared_client *);
/* Must be called before any hardware write; no software reset clears it. */
int mt8167_audio_shared_quarantine(struct mt8167_audio_shared_client *);
int mt8167_audio_shared_detach(struct mt8167_audio_shared_client *);
#ifdef __cplusplus
}
#endif
#endif
