#include <cassert>
#include "irq_windows.h"
_KINTERRUPT test_interrupt{};
static int pending, fail, calls, fault_seen, total, starts, stops;
extern "C" int fake_dummy(void) { return 0; }
extern "C" int mt8167_dl1_interrupt(struct mt8167_dl1_engine *, int *notify) { if(fail)return -1; *notify=pending;return pending; }
extern "C" int mt8167_dl1_start(struct mt8167_dl1_engine *,const struct mt8167_dl1_plan *) { starts++;return 0; }
extern "C" int mt8167_dl1_stop(struct mt8167_dl1_engine *) { stops++;return 0; }
static void notify(void *, ULONG count, BOOLEAN fault) { calls++;total+=(int)count;fault_seen=fault; }
int main() {
 Mt8167Irq irq{}; mt8167_dl1_engine engine{};
 assert(Mt8167ConnectIrq(&irq,&engine,152,4,1,LevelSensitive,notify,nullptr)==0);
 assert(!test_interrupt.fn(&test_interrupt,test_interrupt.ctx) && !irq.dpc.queued);
 struct mt8167_dl1_plan plan{}; assert(!Mt8167SynchronizedStart(&irq,&plan) && starts==1);
 pending=1; assert(test_interrupt.fn(&test_interrupt,test_interrupt.ctx) && irq.dpc.queued);
 assert(test_interrupt.fn(&test_interrupt,test_interrupt.ctx) && irq.dpc.queued);
 irq.dpc.queued=0; irq.dpc.fn(&irq.dpc,irq.dpc.ctx,nullptr,nullptr); assert(calls==1 && total==2 && !fault_seen);
 fail=1; assert(!test_interrupt.fn(&test_interrupt,test_interrupt.ctx) && irq.dpc.queued);
 irq.dpc.queued=0; irq.dpc.fn(&irq.dpc,irq.dpc.ctx,nullptr,nullptr); assert(calls==2 && fault_seen);
 assert(Mt8167SynchronizedStart(&irq,&plan)==-1 && starts==1);
 assert(!Mt8167SynchronizedStop(&irq) && stops==1);
 pending=1;fail=0;assert(test_interrupt.fn(&test_interrupt,test_interrupt.ctx));
 Mt8167DisconnectIrq(&irq); assert(!irq.interrupt && !irq.dpc.queued && !test_interrupt.fn);
}
