#ifndef MT8167_AUDIO_IRQ_WINDOWS_H
#define MT8167_AUDIO_IRQ_WINDOWS_H
#include <ntddk.h>
#include "dl1_engine.h"
/* Experimental, unconnected. Parent must validate translated PnP IRQ and AFE
 * exclusivity, clock ownership, keep engine/mapping alive and serialize all
 * engine accesses via the interrupt synchronization mechanism. */
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
typedef void (*MT8167_IRQ_NOTIFY)(void *context, ULONG servicedIrqs, BOOLEAN fault);
struct Mt8167Irq {
    PKINTERRUPT interrupt;
    KDPC dpc;
    volatile LONG active, fault, pending, generation, armed;
    uint32_t stoppedCurrent;
    struct mt8167_dl1_engine *engine;
    MT8167_IRQ_NOTIFY notify;
    void *context;
};
NTSTATUS Mt8167ConnectIrq(Mt8167Irq *irq, struct mt8167_dl1_engine *engine,
    ULONG vector, KIRQL level, KAFFINITY affinity, KINTERRUPT_MODE mode,
    MT8167_IRQ_NOTIFY notify, void *context);
/* Start/stop serialize with the ISR. A stop success is NOT a DMA drain proof.
 * Caller keeps the interrupt connected and excludes concurrent disconnect. */
int Mt8167SynchronizedStart(Mt8167Irq *irq, const struct mt8167_dl1_plan *plan);
int Mt8167SynchronizedStop(Mt8167Irq *irq);
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
int Mt8167SynchronizedRestart(Mt8167Irq *,const struct mt8167_dl1_plan *,BOOLEAN reset);
/* PASSIVE_LEVEL, owner mutex held; successful register stop required.
 * Drains software ISR/DPC work, never claims outstanding DMA bus drain. */
int Mt8167DrainStoppedIrq(Mt8167Irq *);
#endif
/* Nonblocking, callable at IRQL <= the interrupt synchronization level.
 * Caller excludes disconnect and keeps plan, engine and mapping alive. CUR is
 * accepted only while DL1 is running and within the published DMA ring. */
int Mt8167SynchronizedPosition(Mt8167Irq *irq,const struct mt8167_dl1_plan *plan,uint32_t *offset);
/* PASSIVE_LEVEL only. Caller must retain engine, callbacks and mapping until
 * disconnect and DPC flush complete, and separately prove DMA has drained. */
void Mt8167DisconnectIrq(Mt8167Irq *irq);
#endif
#endif
