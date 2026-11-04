#include "irq_windows.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
static void Mt8167IrqDpc(PKDPC dpc, PVOID context, PVOID a1, PVOID a2)
{
    Mt8167Irq *irq = static_cast<Mt8167Irq *>(context);
    UNREFERENCED_PARAMETER(dpc); UNREFERENCED_PARAMETER(a1); UNREFERENCED_PARAMETER(a2);
    LONG count = InterlockedExchange(&irq->pending, 0);
    BOOLEAN fault = InterlockedCompareExchange(&irq->fault, 0, 0) != 0;
    if (InterlockedCompareExchange(&irq->active, 0, 0) && irq->notify && (count || fault))
        irq->notify(irq->context, (ULONG)count, fault);
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
    if (notify) {
        LONG old;
        do {
            old = InterlockedCompareExchange(&irq->pending, 0, 0);
            if (old == 0x7fffffffL) { InterlockedExchange(&irq->fault, 1); break; }
        } while (InterlockedCompareExchange(&irq->pending, old + 1, old) != old);
        KeInsertQueueDpc(&irq->dpc, NULL, NULL);
    }
    return result > 0 ? TRUE : FALSE;
}
NTSTATUS Mt8167ConnectIrq(Mt8167Irq *irq, struct mt8167_dl1_engine *engine,
    ULONG vector, KIRQL level, KAFFINITY affinity, KINTERRUPT_MODE mode,
    MT8167_IRQ_NOTIFY notify, void *context)
{
    NTSTATUS status;
    if (!irq || !engine || !notify || !affinity || irq->interrupt) return STATUS_INVALID_PARAMETER;
    irq->engine = engine; irq->notify = notify; irq->context = context;
    irq->fault = 0; irq->pending = 0; irq->active = 0;
    KeInitializeDpc(&irq->dpc, Mt8167IrqDpc, irq);
    /* Set active before connect because delivery can begin before it returns. */
    InterlockedExchange(&irq->active, 1);
    status = IoConnectInterrupt(&irq->interrupt, Mt8167Isr, irq, NULL, vector,
        level, level, mode, FALSE, affinity, FALSE);
    if (!NT_SUCCESS(status)) { InterlockedExchange(&irq->active, 0); irq->interrupt = NULL; }
    return status;
}
struct Mt8167SyncCall {
    Mt8167Irq *irq;
    const struct mt8167_dl1_plan *plan;
    int result;
};
static BOOLEAN Mt8167StartLocked(PVOID context)
{
    Mt8167SyncCall *call = static_cast<Mt8167SyncCall *>(context);
    if (!InterlockedCompareExchange(&call->irq->active, 0, 0) ||
        InterlockedCompareExchange(&call->irq->fault, 0, 0)) return FALSE;
    call->result = mt8167_dl1_start(call->irq->engine, call->plan);
    return TRUE;
}
static BOOLEAN Mt8167StopLocked(PVOID context)
{
    Mt8167SyncCall *call = static_cast<Mt8167SyncCall *>(context);
    if (!InterlockedCompareExchange(&call->irq->active, 0, 0)) return FALSE;
    call->result = mt8167_dl1_stop(call->irq->engine);
    return TRUE;
}
int Mt8167SynchronizedStart(Mt8167Irq *irq, const struct mt8167_dl1_plan *plan)
{
    if (!irq || !irq->interrupt || !irq->engine || !plan) return -1;
    Mt8167SyncCall call = {irq, plan, -1};
    if (!KeSynchronizeExecution(irq->interrupt, Mt8167StartLocked, &call)) return -1;
    return call.result;
}
int Mt8167SynchronizedStop(Mt8167Irq *irq)
{
    if (!irq || !irq->interrupt || !irq->engine) return -1;
    Mt8167SyncCall call = {irq, NULL, -1};
    if (!KeSynchronizeExecution(irq->interrupt, Mt8167StopLocked, &call)) return -1;
    return call.result;
}
void Mt8167DisconnectIrq(Mt8167Irq *irq)
{
    if (!irq || !irq->interrupt) return;
    InterlockedExchange(&irq->active, 0);
    IoDisconnectInterrupt(irq->interrupt);
    irq->interrupt = NULL;
    KeRemoveQueueDpc(&irq->dpc);
    KeFlushQueuedDpcs();
    irq->engine = NULL; irq->notify = NULL; irq->context = NULL; irq->pending = 0;
}
#endif
