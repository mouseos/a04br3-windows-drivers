#ifndef MT8167_AUDIO_IRQ_WINDOWS_H
#define MT8167_AUDIO_IRQ_WINDOWS_H
#include <ntddk.h>
#include "dl1_engine.h"
/* Experimental, unconnected. Parent must validate translated PnP IRQ and AFE
 * exclusivity, clock ownership, keep engine/mapping alive and serialize all
 * engine accesses via the interrupt synchronization mechanism. */
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
typedef void (*MT8167_IRQ_NOTIFY)(void *context, BOOLEAN fault);
struct Mt8167Irq {
    PKINTERRUPT interrupt;
    KDPC dpc;
    volatile LONG active, fault;
    struct mt8167_dl1_engine *engine;
    MT8167_IRQ_NOTIFY notify;
    void *context;
};
NTSTATUS Mt8167ConnectIrq(Mt8167Irq *irq, struct mt8167_dl1_engine *engine,
    ULONG vector, KIRQL level, KAFFINITY affinity, KINTERRUPT_MODE mode,
    MT8167_IRQ_NOTIFY notify, void *context);
/* PASSIVE_LEVEL only. Caller must retain engine, callbacks and mapping until
 * disconnect and DPC flush complete, and separately prove DMA has drained. */
void Mt8167DisconnectIrq(Mt8167Irq *irq);
#endif
#endif
