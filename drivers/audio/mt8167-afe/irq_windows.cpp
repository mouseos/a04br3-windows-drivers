#include "irq_windows.h"
#if defined(A04_AUDIO_DIAGNOSTIC_TRACE) && A04_AUDIO_DIAGNOSTIC_TRACE
#define A04_TRACE(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,__VA_ARGS__)
#else
#define A04_TRACE(...) ((void)0)
#endif
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
    /* Delivery may start immediately after IoConnectInterrupt, before DL1
     * is started, or after a synchronized stop. Neither is an engine fault. */
    if (irq->engine->state == MT8167_DL1_IDLE ||
        irq->engine->state == MT8167_DL1_STOPPED) return FALSE;
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
#if defined(A04_AUDIO_DIAGNOSTIC_TRACE) && A04_AUDIO_DIAGNOSTIC_TRACE
    uint32_t dac,irqControl,status,current;int observed;
#endif
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
#if defined(A04_AUDIO_DIAGNOSTIC_TRACE) && A04_AUDIO_DIAGNOSTIC_TRACE
    if (!call->result && call->irq->engine->io.read32) {
        struct mt8167_dl1_io *io=&call->irq->engine->io;
        call->observed=!io->read32(io->context,0x10,&call->dac) &&
            !io->read32(io->context,0x3a0,&call->irqControl) &&
            !io->read32(io->context,0x3a4,&call->status) &&
            !io->read32(io->context,0x44,&call->current);
    }
#endif
    return TRUE;
}
struct Mt8167PositionCall {
    Mt8167Irq *irq;
    const struct mt8167_dl1_plan *plan;
    uint32_t offset;
    int result;
};
static BOOLEAN Mt8167PositionLocked(PVOID context)
{
    Mt8167PositionCall *call=static_cast<Mt8167PositionCall *>(context);
    struct mt8167_dl1_engine *engine=call->irq->engine;
    uint32_t current;
    if (!InterlockedCompareExchange(&call->irq->active,0,0) ||
        InterlockedCompareExchange(&call->irq->fault,0,0) ||
        !engine || engine->state!=MT8167_DL1_RUNNING || !engine->io.read32)
        return FALSE;
    if (engine->io.read32(engine->io.context,0x44u,&current) ||
        !mt8167_dl1_position(call->plan,current,&call->offset)) return FALSE;
    call->result=0;return TRUE;
}
int Mt8167SynchronizedStart(Mt8167Irq *irq, const struct mt8167_dl1_plan *plan)
{
    if (!irq || !irq->interrupt || !irq->engine || !plan) return -1;
    Mt8167SyncCall call = {}; call.irq=irq;call.plan=plan;call.result=-1;
    if (!KeSynchronizeExecution(irq->interrupt, Mt8167StartLocked, &call)) return -1;
    return call.result;
}
int Mt8167SynchronizedStop(Mt8167Irq *irq)
{
    if (!irq || !irq->interrupt || !irq->engine) return -1;
    Mt8167SyncCall call = {}; call.irq=irq;call.result=-1;
    if (!KeSynchronizeExecution(irq->interrupt, Mt8167StopLocked, &call)) return -1;
#if defined(A04_AUDIO_DIAGNOSTIC_TRACE) && A04_AUDIO_DIAGNOSTIC_TRACE
    A04_TRACE("A04AUD afe-stop result=%d observed=%d dac=%08lx irq=%08lx status=%08lx cursor-nonzero=%d; drain=unknown\n",call.result,call.observed,(ULONG)call.dac,(ULONG)call.irqControl,(ULONG)call.status,call.current!=0);
#endif
    return call.result;
}
int Mt8167SynchronizedPosition(Mt8167Irq *irq,const struct mt8167_dl1_plan *plan,uint32_t *offset)
{
    if (!irq || !irq->interrupt || !irq->engine || !plan || !offset) return -1;
    Mt8167PositionCall call={irq,plan,0,-1};
    if (!KeSynchronizeExecution(irq->interrupt,Mt8167PositionLocked,&call)) return -1;
    if (call.result) return -1;
    *offset=call.offset;return 0;
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
