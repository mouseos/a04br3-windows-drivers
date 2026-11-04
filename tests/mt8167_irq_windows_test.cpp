#include <cassert>
#include "irq_windows.h"
_KINTERRUPT test_interrupt{};
static int pending, fail, calls, fault_seen;
extern "C" int fake_dummy(void) { return 0; }
extern "C" int mt8167_dl1_interrupt(struct mt8167_dl1_engine *, int *notify) { if(fail)return -1; *notify=pending;return pending; }
static void notify(void *, BOOLEAN fault) { calls++;fault_seen=fault; }
int main() {
 Mt8167Irq irq{}; mt8167_dl1_engine engine{};
 assert(Mt8167ConnectIrq(&irq,&engine,152,4,1,LevelSensitive,notify,nullptr)==0);
 assert(!test_interrupt.fn(&test_interrupt,test_interrupt.ctx) && !irq.dpc.queued);
 pending=1; assert(test_interrupt.fn(&test_interrupt,test_interrupt.ctx) && irq.dpc.queued);
 irq.dpc.queued=0; irq.dpc.fn(&irq.dpc,irq.dpc.ctx,nullptr,nullptr); assert(calls==1 && !fault_seen);
 fail=1; assert(!test_interrupt.fn(&test_interrupt,test_interrupt.ctx) && irq.dpc.queued);
 irq.dpc.queued=0; irq.dpc.fn(&irq.dpc,irq.dpc.ctx,nullptr,nullptr); assert(calls==2 && fault_seen);
 pending=1;fail=0;assert(test_interrupt.fn(&test_interrupt,test_interrupt.ctx));
 Mt8167DisconnectIrq(&irq); assert(!irq.interrupt && !irq.dpc.queued && !test_interrupt.fn);
}
