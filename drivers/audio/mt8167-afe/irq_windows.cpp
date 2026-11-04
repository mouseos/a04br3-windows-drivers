#include "irq_windows.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
static void Mt8167IrqDpc(PKDPC dpc, PVOID context, PVOID a1, PVOID a2)
{
    Mt8167Irq *irq = static_cast<Mt8167Irq *>(context);
    UNREFERENCED_PARAMETER(dpc); UNREFERENCED_PARAMETER(a1); UNREFERENCED_PARAMETER(a2);
    if (InterlockedCompareExchange(&irq->active, 0, 0) && irq->notify)
        irq->notify(irq->context, InterlockedCompareExchange(&irq->fault, 0, 0) != 0);
}
static BOOLEAN Mt8167Isr(PKINTERRUPT interrupt, PVOID context)
{
    Mt8167Irq *irq = static_cast<Mt8167Irq *>(context);
    int notify = 0, result;
    UNREFERENCED_PARAMETER(interrupt);
    if (!InterlockedCompareExchange(&irq->active, 0, 0)) return FALSE;
    result = mt8167_dl1_interrupt(irq->engine, &notify);
    if (result < 0) {
        InterlockedExchange(&irq->fault, 1);
        KeInsertQueueDpc(&irq->dpc, NULL, NULL);
        return FALSE;
    }
    if (notify) KeInsertQueueDpc(&irq->dpc, NULL, NULL);
    return result > 0 ? TRUE : FALSE;
}
NTSTATUS Mt8167ConnectIrq(Mt8167Irq *irq, struct mt8167_dl1_engine *engine,
    ULONG vector, KIRQL level, KAFFINITY affinity, KINTERRUPT_MODE mode,
    MT8167_IRQ_NOTIFY notify, void *context)
{
    NTSTATUS status;
    if (!irq || !engine || !notify || !affinity || irq->interrupt) return STATUS_INVALID_PARAMETER;
    irq->engine = engine; irq->notify = notify; irq->context = context;
    irq->fault = 0; irq->active = 0;
    KeInitializeDpc(&irq->dpc, Mt8167IrqDpc, irq);
    /* Set active before connect because delivery can begin before it returns. */
    InterlockedExchange(&irq->active, 1);
    status = IoConnectInterrupt(&irq->interrupt, Mt8167Isr, irq, NULL, vector,
        level, level, mode, FALSE, affinity, FALSE);
    if (!NT_SUCCESS(status)) { InterlockedExchange(&irq->active, 0); irq->interrupt = NULL; }
    return status;
}
void Mt8167DisconnectIrq(Mt8167Irq *irq)
{
    if (!irq || !irq->interrupt) return;
    InterlockedExchange(&irq->active, 0);
    IoDisconnectInterrupt(irq->interrupt);
    irq->interrupt = NULL;
    KeRemoveQueueDpc(&irq->dpc);
    KeFlushQueuedDpcs();
    irq->engine = NULL; irq->notify = NULL; irq->context = NULL;
}
#endif
